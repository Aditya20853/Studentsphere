
#include "httplib.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// Student structure
struct Student {
    string rollNo;
    string name;
    int age;
    string course;
    int marks;
};

// File used for permanent storage
const string DATA_FILE = "students.txt";

// Student records loaded into memory
vector<Student> students;

// --------------------------------------------------
// DEFAULT STUDENT RECORDS
// --------------------------------------------------

vector<Student> getDefaultStudents() {
    return {
        {"101", "Rahul Sharma", 22, "MCA", 85},
        {"102", "Priya Singh", 23, "MCA", 92},
        {"103", "Amit Kumar", 22, "MCA", 45},
        {"104", "Neha Verma", 24, "MCA", 78}
    };
}

// --------------------------------------------------
// SAVE STUDENTS PERMANENTLY TO FILE
// --------------------------------------------------

bool saveStudents() {
    ofstream file(DATA_FILE, ios::trunc);

    if (!file.is_open()) {
        cerr << "Error: Could not open " << DATA_FILE
             << " for saving.\n";
        return false;
    }

    // Format:
    // rollNo|name|age|course|marks
    for (const Student& s : students) {
        file << s.rollNo << "|"
             << s.name << "|"
             << s.age << "|"
             << s.course << "|"
             << s.marks << "\n";
    }

    file.close();

    return !file.fail();
}

// --------------------------------------------------
// LOAD STUDENTS FROM FILE
// --------------------------------------------------

void loadStudents() {
    ifstream file(DATA_FILE);

    // If the file does not exist, create default records.
    if (!file.is_open()) {
        cout << "First run: Creating default student records.\n";

        students = getDefaultStudents();

        if (saveStudents()) {
            cout << "Default records saved to "
                 << DATA_FILE << ".\n";
        }

        return;
    }

    students.clear();

    string line;

    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        stringstream ss(line);

        string rollNo;
        string name;
        string ageText;
        string course;
        string marksText;

        // Read fields separated by |
        if (!getline(ss, rollNo, '|')) continue;
        if (!getline(ss, name, '|')) continue;
        if (!getline(ss, ageText, '|')) continue;
        if (!getline(ss, course, '|')) continue;
        if (!getline(ss, marksText, '|')) continue;

        try {
            size_t agePos = 0;
            size_t marksPos = 0;

            int age = stoi(ageText, &agePos);
            int marks = stoi(marksText, &marksPos);

            if (agePos != ageText.size() ||
                marksPos != marksText.size() ||
                age <= 0 ||
                marks < 0 ||
                marks > 100 ||
                rollNo.empty()) {
                continue;
            }

            // Prevent duplicate roll numbers in loaded data.
            bool duplicate = false;

            for (const Student& existing : students) {
                if (existing.rollNo == rollNo) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                students.push_back({
                    rollNo, name, age, course, marks
                });
            }

        } catch (...) {
            // Ignore invalid lines in the file.
            continue;
        }
    }

    file.close();

    cout << "Loaded " << students.size()
         << " student record(s) from "
         << DATA_FILE << ".\n";
}

// --------------------------------------------------
// JSON HELPER FUNCTIONS
// --------------------------------------------------

string jsonEscape(const string& input) {
    string output;

    for (unsigned char c : input) {
        switch (c) {
            case '"':
                output += "\\\"";
                break;

            case '\\':
                output += "\\\\";
                break;

            case '\n':
                output += "\\n";
                break;

            case '\r':
                output += "\\r";
                break;

            case '\t':
                output += "\\t";
                break;

            default:
                if (c < 32) {
                    const char* hex = "0123456789abcdef";

                    output += "\\u00";
                    output += hex[(c >> 4) & 0x0F];
                    output += hex[c & 0x0F];
                } else {
                    output += static_cast<char>(c);
                }
        }
    }

    return output;
}

string studentToJson(const Student& s) {
    return
        "{\"rollNo\":\"" + jsonEscape(s.rollNo) +
        "\",\"name\":\"" + jsonEscape(s.name) +
        "\",\"age\":" + to_string(s.age) +
        ",\"course\":\"" + jsonEscape(s.course) +
        "\",\"marks\":" + to_string(s.marks) + "}";
}

string allStudentsToJson() {
    string result = "[";

    for (size_t i = 0; i < students.size(); i++) {
        if (i > 0) {
            result += ",";
        }

        result += studentToJson(students[i]);
    }

    result += "]";
    return result;
}

void sendError(httplib::Response& res,
               int status,
               const string& message) {
    res.status = status;
    res.set_content(
        "{\"error\":\"" + jsonEscape(message) + "\"}",
        "application/json"
    );
}

// --------------------------------------------------
// CORS: ALLOWS THE WEBPAGE TO CONNECT TO C++
// --------------------------------------------------

void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header(
        "Access-Control-Allow-Methods",
        "GET, POST, PUT, DELETE, OPTIONS"
    );
    res.set_header(
        "Access-Control-Allow-Headers",
        "Content-Type"
    );
}

// --------------------------------------------------
// VALIDATION HELPERS
// --------------------------------------------------

bool hasUnsafeCharacters(const string& value) {
    // The | character separates fields in students.txt.
    return value.find('|') != string::npos ||
           value.find('\n') != string::npos ||
           value.find('\r') != string::npos;
}

bool readIntegerParam(
    const httplib::Request& req,
    const string& key,
    int& result
) {
    if (!req.has_param(key)) {
        return false;
    }

    string value = req.get_param_value(key);

    try {
        size_t pos = 0;
        result = stoi(value, &pos);

        return pos == value.size();
    } catch (...) {
        return false;
    }
}

bool readStudentParams(
    const httplib::Request& req,
    Student& s,
    string& error
) {
    if (!req.has_param("rollNo") ||
        !req.has_param("name") ||
        !req.has_param("course")) {
        error = "Please provide rollNo, name and course.";
        return false;
    }

    s.rollNo = req.get_param_value("rollNo");
    s.name = req.get_param_value("name");
    s.course = req.get_param_value("course");

    if (s.rollNo.empty() || s.name.empty() ||
        s.course.empty()) {
        error = "Student fields cannot be empty.";
        return false;
    }

    if (hasUnsafeCharacters(s.rollNo) ||
        hasUnsafeCharacters(s.name) ||
        hasUnsafeCharacters(s.course)) {
        error = "The characters | and line breaks are not allowed.";
        return false;
    }

    if (!readIntegerParam(req, "age", s.age) ||
        !readIntegerParam(req, "marks", s.marks)) {
        error = "Age and marks must be valid whole numbers.";
        return false;
    }

    if (s.age <= 0 || s.marks < 0 || s.marks > 100) {
        error = "Age must be positive and marks must be between 0 and 100.";
        return false;
    }

    return true;
}

// --------------------------------------------------
// MAIN SERVER
// --------------------------------------------------

int main() {
    // Load saved data before accepting requests.
    loadStudents();

    httplib::Server server;

    // Handle browser CORS preflight requests.
    server.Options("/api/students",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);
            res.status = 204;
        }
    );

    // Server status
    server.Get("/api/status",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);

            res.set_content(
                "{\"message\":\"StudentSphere C++ server is working!\"}",
                "application/json"
            );
        }
    );

    // GET: Display all student records
    server.Get("/api/students",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);

            res.set_content(
                allStudentsToJson(),
                "application/json"
            );
        }
    );

    // POST: Add a student
    server.Post("/api/students",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);

            Student s;
            string error;

            if (!readStudentParams(req, s, error)) {
                sendError(res, 400, error);
                return;
            }

            for (const Student& existing : students) {
                if (existing.rollNo == s.rollNo) {
                    sendError(res, 409,
                              "This roll number already exists.");
                    return;
                }
            }

            students.push_back(s);

            if (!saveStudents()) {
                students.pop_back();
                sendError(res, 500,
                          "Could not save student data to file.");
                return;
            }

            res.status = 201;
            res.set_content(
                studentToJson(s),
                "application/json"
            );
        }
    );

    // PUT: Update an existing student
    server.Put("/api/students",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);

            Student updated;
            string error;

            if (!readStudentParams(req, updated, error)) {
                sendError(res, 400, error);
                return;
            }

            auto it = find_if(
                students.begin(),
                students.end(),
                [&](const Student& s) {
                    return s.rollNo == updated.rollNo;
                }
            );

            if (it == students.end()) {
                sendError(res, 404, "Student not found.");
                return;
            }

            Student previous = *it;
            *it = updated;

            if (!saveStudents()) {
                *it = previous;
                sendError(res, 500,
                          "Could not save updated student data.");
                return;
            }

            res.set_content(
                studentToJson(updated),
                "application/json"
            );
        }
    );

    // DELETE: Remove a student
    server.Delete("/api/students",
        [](const httplib::Request& req,
           httplib::Response& res) {
            setCORS(res);

            if (!req.has_param("rollNo")) {
                sendError(res, 400, "Please provide rollNo.");
                return;
            }

            string rollNo = req.get_param_value("rollNo");

            auto it = find_if(
                students.begin(),
                students.end(),
                [&](const Student& s) {
                    return s.rollNo == rollNo;
                }
            );

            if (it == students.end()) {
                sendError(res, 404, "Student not found.");
                return;
            }

            size_t index = static_cast<size_t>(
                distance(students.begin(), it)
            );

            Student removed = *it;
            students.erase(it);

            if (!saveStudents()) {
                students.insert(
                    students.begin() + index,
                    removed
                );

                sendError(res, 500,
                          "Could not save changes to file.");
                return;
            }

            res.set_content(
                "{\"message\":\"Student deleted successfully.\"}",
                "application/json"
            );
        }
    );

    cout << "\n====================================\n";
    cout << " StudentSphere C++ Server\n";
    cout << " Permanent file storage enabled\n";
    cout << " URL: http://localhost:8080\n";
    cout << " Data file: " << DATA_FILE << "\n";
    cout << " Press Ctrl+C to stop the server\n";
    cout << "====================================\n\n";

    if (!server.listen("127.0.0.1", 8080)) {
        cerr << "Could not start server on port 8080.\n";
        cerr << "Check whether another server is already running.\n";
        return 1;
    }

    return 0;
}
