#include <iostream>
#include <mysql/mysql.h>

int main() {
    MYSQL* conn = mysql_init(nullptr);

    if (!conn) {
        std::cout << "MySQL initialization failed.\n";
        return 1;
    }

    if (mysql_real_connect(
            conn, "127.0.0.1", "root", "Aditya@123",
            "studentsphere", 3306, nullptr, 0)) {
        std::cout << "Connected to StudentSphere database!\n";
        mysql_close(conn);
        return 0;
    }

    std::cout << "Connection failed: "
              << mysql_error(conn) << '\n';
    mysql_close(conn);
    return 1;
}