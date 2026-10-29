
#include "httplib.h"
#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <mutex>

using namespace std;

struct Student {
    string rollNo, name, course;
    int age, marks;
};

MYSQL* db = nullptr;
mutex dbMutex;

string escapeSQL(const string& value) {
    string out(value.size() * 2 + 1, '\0');
    unsigned long n = mysql_real_escape_string(
        db, &out[0], value.c_str(),
        static_cast<unsigned long>(value.size()));
    out.resize(n);
    return out;
}

bool query(const string& sql) {
    if (mysql_query(db, sql.c_str()) != 0) {
        cerr << "MySQL error: " << mysql_error(db) << '\n';
        return false;
    }
    return true;
}

void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

string jsonEscape(const string& s) {
    string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 15];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

string toJson(const Student& s) {
    return "{\"rollNo\":\"" + jsonEscape(s.rollNo) +
           "\",\"name\":\"" + jsonEscape(s.name) +
           "\",\"age\":" + to_string(s.age) +
           ",\"course\":\"" + jsonEscape(s.course) +
           "\",\"marks\":" + to_string(s.marks) + "}";
}

void errorResponse(httplib::Response& res, int status,
                   const string& message) {
    res.status = status;
    res.set_content("{\"error\":\"" + jsonEscape(message) + "\"}",
                    "application/json");
}

bool readStudent(const httplib::Request& req, Student& s,
                 string& error) {
    if (!req.has_param("rollNo") ||
        !req.has_param("name") ||
        !req.has_param("course")) {
        error = "Please provide rollNo, name and course.";
        return false;
    }

    s.rollNo = req.get_param_value("rollNo");
    s.name = req.get_param_value("name");
    s.course = req.get_param_value("course");

    if (s.rollNo.empty() || s.name.empty() || s.course.empty()) {
        error = "Student fields cannot be empty.";
        return false;
    }

    if (s.rollNo.find_first_of("\r\n") != string::npos ||
        s.name.find_first_of("\r\n") != string::npos ||
        s.course.find_first_of("\r\n") != string::npos) {
        error = "Line breaks are not allowed.";
        return false;
    }

    try {
        size_t p1 = 0, p2 = 0;
        string age = req.get_param_value("age");
        string marks = req.get_param_value("marks");
        s.age = stoi(age, &p1);
        s.marks = stoi(marks, &p2);

        if (p1 != age.size() || p2 != marks.size())
            throw invalid_argument("invalid number");
    } catch (...) {
        error = "Age and marks must be whole numbers.";
        return false;
    }

    if (s.age <= 0 || s.marks < 0 || s.marks > 100) {
        error = "Age must be positive and marks must be between 0 and 100.";
        return false;
    }
    return true;
}

// Call with dbMutex already locked.
bool exists(const string& rollNo) {
    string sql = "SELECT roll_no FROM students WHERE roll_no='" +
                 escapeSQL(rollNo) + "' LIMIT 1";
    if (!query(sql)) return false;

    MYSQL_RES* result = mysql_store_result(db);
    if (!result) return false;

    bool found = mysql_num_rows(result) > 0;
    mysql_free_result(result);
    return found;
}

bool loadStudents(vector<Student>& list) {
    lock_guard<mutex> lock(dbMutex);
    list.clear();

    if (!query("SELECT roll_no,name,age,course,marks "
               "FROM students ORDER BY roll_no"))
        return false;

    MYSQL_RES* result = mysql_store_result(db);
    if (!result) return false;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Student s;
        s.rollNo = row[0] ? row[0] : "";
        s.name = row[1] ? row[1] : "";
        s.age = row[2] ? atoi(row[2]) : 0;
        s.course = row[3] ? row[3] : "";
        s.marks = row[4] ? atoi(row[4]) : 0;
        list.push_back(s);
    }

    mysql_free_result(result);
    return true;
}

int main() {
    const char* user = getenv("STUDENTSPHERE_DB_USER");
    const char* password = getenv("STUDENTSPHERE_DB_PASSWORD");

    if (!user || !*user) user = "root";

    if (!password || !*password) {
        cerr << "Set STUDENTSPHERE_DB_PASSWORD before starting.\n";
        return 1;
    }

    db = mysql_init(nullptr);
    if (!db) {
        cerr << "MySQL initialization failed.\n";
        return 1;
    }

    if (!mysql_real_connect(db, "127.0.0.1", user, password,
                            "studentsphere", 3306, nullptr, 0)) {
        cerr << "MySQL connection failed: "
             << mysql_error(db) << '\n';
        mysql_close(db);
        return 1;
    }

    mysql_set_character_set(db, "utf8mb4");
    cout << "Connected to studentsphere database.\n";

    // Insert sample records only when the table is empty.
    {
        lock_guard<mutex> lock(dbMutex);
        if (!query("SELECT COUNT(*) FROM students")) {
            mysql_close(db);
            return 1;
        }

        MYSQL_RES* result = mysql_store_result(db);
        if (!result) {
            mysql_close(db);
            return 1;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        unsigned long long count =
            (row && row[0]) ? strtoull(row[0], nullptr, 10) : 0;
        mysql_free_result(result);

        if (count == 0) {
            const char* samples[] = {
                "INSERT INTO students (roll_no,name,age,course,marks) VALUES ('101','Rahul Sharma',22,'MCA',85)",
                "INSERT INTO students (roll_no,name,age,course,marks) VALUES ('102','Priya Singh',23,'MCA',92)",
                "INSERT INTO students (roll_no,name,age,course,marks) VALUES ('103','Amit Kumar',22,'MCA',45)",
                "INSERT INTO students (roll_no,name,age,course,marks) VALUES ('104','Neha Verma',24,'MCA',78)"
            };

            for (const char* sql : samples) {
                if (!query(sql)) {
                    mysql_close(db);
                    return 1;
                }
            }
            cout << "Sample records added to MySQL.\n";
        }
    }

    httplib::Server server;

    server.Options("/api/students",
        [](const httplib::Request&, httplib::Response& res) {
            setCORS(res);
            res.status = 204;
        });

    server.Get("/api/status",
        [](const httplib::Request&, httplib::Response& res) {
            setCORS(res);
            res.set_content(
                "{\"message\":\"StudentSphere C++ server is working with MySQL!\"}",
                "application/json");
        });

    server.Get("/api/students",
        [](const httplib::Request&, httplib::Response& res) {
            setCORS(res);
            vector<Student> list;

            if (!loadStudents(list)) {
                errorResponse(res, 500, "Could not read students from MySQL.");
                return;
            }

            string json = "[";
            for (size_t i = 0; i < list.size(); ++i) {
                if (i) json += ",";
                json += toJson(list[i]);
            }
            json += "]";
            res.set_content(json, "application/json");
        });

    server.Post("/api/students",
        [](const httplib::Request& req, httplib::Response& res) {
            setCORS(res);
            Student s{};
            string error;

            if (!readStudent(req, s, error)) {
                errorResponse(res, 400, error);
                return;
            }

            lock_guard<mutex> lock(dbMutex);
            if (exists(s.rollNo)) {
                errorResponse(res, 409, "This roll number already exists.");
                return;
            }

            string sql =
                "INSERT INTO students (roll_no,name,age,course,marks) VALUES ('" +
                escapeSQL(s.rollNo) + "','" + escapeSQL(s.name) + "'," +
                to_string(s.age) + ",'" + escapeSQL(s.course) + "'," +
                to_string(s.marks) + ")";

            if (!query(sql)) {
                errorResponse(res, 500, "Could not add student to MySQL.");
                return;
            }

            res.status = 201;
            res.set_content(toJson(s), "application/json");
        });

    server.Put("/api/students",
        [](const httplib::Request& req, httplib::Response& res) {
            setCORS(res);
            Student s{};
            string error;

            if (!readStudent(req, s, error)) {
                errorResponse(res, 400, error);
                return;
            }

            lock_guard<mutex> lock(dbMutex);
            if (!exists(s.rollNo)) {
                errorResponse(res, 404, "Student not found.");
                return;
            }

            string sql =
                "UPDATE students SET name='" + escapeSQL(s.name) +
                "', age=" + to_string(s.age) +
                ", course='" + escapeSQL(s.course) +
                "', marks=" + to_string(s.marks) +
                " WHERE roll_no='" + escapeSQL(s.rollNo) + "'";

            if (!query(sql)) {
                errorResponse(res, 500, "Could not update student in MySQL.");
                return;
            }

            res.set_content(toJson(s), "application/json");
        });

    server.Delete("/api/students",
        [](const httplib::Request& req, httplib::Response& res) {
            setCORS(res);

            if (!req.has_param("rollNo") ||
                req.get_param_value("rollNo").empty()) {
                errorResponse(res, 400, "Please provide rollNo.");
                return;
            }

            string rollNo = req.get_param_value("rollNo");
            lock_guard<mutex> lock(dbMutex);

            if (!exists(rollNo)) {
                errorResponse(res, 404, "Student not found.");
                return;
            }

            string sql = "DELETE FROM students WHERE roll_no='" +
                         escapeSQL(rollNo) + "'";

            if (!query(sql)) {
                errorResponse(res, 500, "Could not delete student from MySQL.");
                return;
            }

            res.set_content(
                "{\"message\":\"Student deleted successfully.\"}",
                "application/json");
        });

    cout << "StudentSphere server: http://localhost:8080\n";
    cout << "Storage: MySQL (studentsphere)\n";
    cout << "Press Ctrl+C to stop.\n";

    if (!server.listen("127.0.0.1", 8080)) {
        cerr << "Could not start server. Check whether port 8080 is already in use.\n";
        mysql_close(db);
        return 1;
    }

    mysql_close(db);
    return 0;
}
