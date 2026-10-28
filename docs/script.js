
document.addEventListener("DOMContentLoaded", function () {
    const API_URL = "http://localhost:8080/api/students";

    const form = document.getElementById("studentForm");
    const rollInput = document.getElementById("rollNo");
    const nameInput = document.getElementById("studentName");
    const ageInput = document.getElementById("studentAge");
    const courseInput = document.getElementById("studentCourse");
    const marksInput = document.getElementById("studentMarks");
    const tableBody = document.getElementById("studentTableBody");
    const searchInput = document.getElementById("studentSearch");
    const saveButton = document.getElementById("saveStudentButton");
    const formTitle = document.getElementById("formTitle");

    let students = [];
    let editingRollNo = null;

    function calculateGrade(marks) {
        if (marks >= 90) return "A";
        if (marks >= 80) return "B";
        if (marks >= 70) return "C";
        if (marks >= 60) return "D";
        if (marks >= 50) return "E";
        return "F";
    }

    function addCell(row, value) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.appendChild(cell);
        return cell;
    }

    // Send requests to the C++ server.
    async function apiRequest(method, params = {}) {
        const query = new URLSearchParams(params).toString();
        const url = query ? `${API_URL}?${query}` : API_URL;

        const response = await fetch(url, { method });
        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Request failed.");
        }

        return data;
    }

    // Load student records from C++.
    async function loadStudentsFromServer() {
        try {
            const data = await apiRequest("GET");

            students = data;
            localStorage.setItem("students", JSON.stringify(students));
            refreshView();
        } catch (error) {
            console.error("C++ connection error:", error);

            // Show cached records if the server is unavailable.
            try {
                students = JSON.parse(
                    localStorage.getItem("students") || "[]"
                );

                if (!Array.isArray(students)) students = [];
            } catch {
                students = [];
            }

            refreshView();
            alert(
                "Cannot connect to C++. Start server.exe and refresh the page."
            );
        }
    }

    function displayStudents(list = students) {
        tableBody.replaceChildren();

        if (list.length === 0) {
            const row = document.createElement("tr");
            const cell = addCell(row, "No students found.");
            cell.colSpan = 8;
            tableBody.appendChild(row);
            return;
        }

        list.forEach(function (student) {
            const row = document.createElement("tr");

            addCell(row, student.rollNo);
            addCell(row, student.name);
            addCell(row, student.age);
            addCell(row, student.course);
            addCell(row, student.marks);

            const grade = calculateGrade(Number(student.marks));
            const gradeCell = addCell(row, "");
            const gradeBadge = document.createElement("span");
            gradeBadge.className = "grade grade-" + grade.toLowerCase();
            gradeBadge.textContent = grade;
            gradeCell.appendChild(gradeBadge);

            const statusCell = addCell(row, "");
            const statusBadge = document.createElement("span");
            const passed = Number(student.marks) >= 50;

            statusBadge.className =
                "status " + (passed ? "passed" : "failed");
            statusBadge.textContent = passed ? "Passed" : "Failed";
            statusCell.appendChild(statusBadge);

            const actionsCell = addCell(row, "");

            const editButton = document.createElement("button");
            editButton.type = "button";
            editButton.className = "action-button edit-button";
            editButton.textContent = "Edit";
            editButton.addEventListener("click", function () {
                editStudent(student.rollNo);
            });

            const deleteButton = document.createElement("button");
            deleteButton.type = "button";
            deleteButton.className = "action-button delete-button";
            deleteButton.textContent = "Delete";
            deleteButton.addEventListener("click", function () {
                deleteStudent(student.rollNo);
            });

            actionsCell.append(editButton, deleteButton);
            tableBody.appendChild(row);
        });
    }

    function updateDashboard() {
        const total = students.length;
        const passed = students.filter(
            student => Number(student.marks) >= 50
        ).length;

        const average = total === 0 ? 0 :
            students.reduce(
                (sum, student) => sum + Number(student.marks), 0
            ) / total;

        const highestMarks = total === 0 ? null :
            Math.max(...students.map(student => Number(student.marks)));

        document.getElementById("totalStudents").textContent = total;
        document.getElementById("passedStudents").textContent = passed;
        document.getElementById("averageMarks").textContent =
            average.toFixed(1) + "%";

        document.getElementById("topGrade").textContent =
            highestMarks === null ? "-" : calculateGrade(highestMarks);
    }

    function refreshView() {
        const query = searchInput.value.trim().toLowerCase();

        const filteredStudents = students.filter(function (student) {
            return (
                String(student.rollNo).toLowerCase().includes(query) ||
                String(student.name).toLowerCase().includes(query) ||
                String(student.course).toLowerCase().includes(query)
            );
        });

        displayStudents(filteredStudents);
        updateDashboard();
    }

    function resetStudentForm() {
        form.reset();
        editingRollNo = null;
        rollInput.readOnly = false;
        formTitle.textContent = "Add New Student";
        saveButton.textContent = "Save Student";
    }

    // Add or update a student through C++.
    form.addEventListener("submit", async function (event) {
        event.preventDefault();

        const rollNo = rollInput.value.trim();
        const name = nameInput.value.trim();
        const age = Number(ageInput.value);
        const course = courseInput.value.trim();
        const marksText = marksInput.value.trim();
        const marks = Number(marksText);

        if (!rollNo || !name || !course || marksText === "") {
            alert("Please fill in all fields.");
            return;
        }

        if (!/^[1-9]\d*$/.test(rollNo)) {
            alert("Roll number must be a positive whole number.");
            return;
        }

        if (!Number.isInteger(age) || age < 1 || age > 120) {
            alert("Please enter a valid age between 1 and 120.");
            return;
        }

        if (!Number.isFinite(marks) || marks < 0 || marks > 100) {
            alert("Marks must be between 0 and 100.");
            return;
        }

        const duplicate = students.some(function (student) {
            return String(student.rollNo) === rollNo &&
                String(student.rollNo) !== String(editingRollNo);
        });

        if (duplicate) {
            alert("This roll number already exists.");
            return;
        }

        const params = { rollNo, name, age, course, marks };
        const method = editingRollNo === null ? "POST" : "PUT";

        try {
            await apiRequest(method, params);
            await loadStudentsFromServer();
            resetStudentForm();
            refreshView();
            alert("Student record saved successfully.");
        } catch (error) {
            alert("Could not save student: " + error.message);
        }
    });

    function editStudent(rollNo) {
        const student = students.find(
            item => String(item.rollNo) === String(rollNo)
        );

        if (!student) return;

        editingRollNo = student.rollNo;
        rollInput.value = student.rollNo;
        nameInput.value = student.name;
        ageInput.value = student.age;
        courseInput.value = student.course;
        marksInput.value = student.marks;

        rollInput.readOnly = true;
        formTitle.textContent = "Edit Student";
        saveButton.textContent = "Update Student";

        document.getElementById("add-student").scrollIntoView({
            behavior: "smooth",
            block: "start"
        });
    }

    // Delete a student through C++.
    async function deleteStudent(rollNo) {
        const student = students.find(
            item => String(item.rollNo) === String(rollNo)
        );

        if (!student) return;

        if (!confirm("Are you sure you want to delete " + student.name + "?")) {
            return;
        }

        try {
            await apiRequest("DELETE", { rollNo });

            if (String(editingRollNo) === String(rollNo)) {
                resetStudentForm();
            }

            await loadStudentsFromServer();
            refreshView();
        } catch (error) {
            alert("Could not delete student: " + error.message);
        }
    }

    searchInput.addEventListener("input", refreshView);

    form.addEventListener("reset", function () {
        editingRollNo = null;
        rollInput.readOnly = false;
        formTitle.textContent = "Add New Student";
        saveButton.textContent = "Save Student";
    });

    // Fetch records from C++ when the page opens.
    loadStudentsFromServer();
});
