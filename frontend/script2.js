
document.addEventListener("DOMContentLoaded", function () {
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

    // Sample student records shown on the first visit.
    const sampleStudents = [
        { rollNo: "101", name: "Rahul Sharma", age: 22, course: "MCA", marks: 85 },
        { rollNo: "102", name: "Priya Singh", age: 23, course: "MCA", marks: 92 },
        { rollNo: "103", name: "Amit Kumar", age: 22, course: "MCA", marks: 45 },
        { rollNo: "104", name: "Neha Verma", age: 24, course: "MCA", marks: 78 }
    ];

    let students = loadStudents();
    let editingRollNo = null;

    // Load saved records from the browser.
    function loadStudents() {
        try {
            const saved = localStorage.getItem("students");

            if (saved === null) {
                localStorage.setItem("students", JSON.stringify(sampleStudents));
                return [...sampleStudents];
            }

            const parsed = JSON.parse(saved);
            return Array.isArray(parsed) ? parsed : [...sampleStudents];
        } catch (error) {
            console.error("Unable to load student records:", error);
            return [...sampleStudents];
        }
    }

    // Save records in localStorage.
    function saveStudents() {
        try {
            localStorage.setItem("students", JSON.stringify(students));
            return true;
        } catch (error) {
            console.error("Unable to save student records:", error);
            alert("The browser could not save the records.");
            return false;
        }
    }

    // Calculate grade based on marks.
    function calculateGrade(marks) {
        if (marks >= 90) return "A";
        if (marks >= 80) return "B";
        if (marks >= 70) return "C";
        if (marks >= 60) return "D";
        if (marks >= 50) return "E";
        return "F";
    }

    // Create a table cell.
    function addCell(row, value) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.appendChild(cell);
        return cell;
    }

    // Display students in the table.
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

            // Grade column
            const grade = calculateGrade(Number(student.marks));
            const gradeCell = addCell(row, "");
            const gradeBadge = document.createElement("span");
            gradeBadge.className = "grade grade-" + grade.toLowerCase();
            gradeBadge.textContent = grade;
            gradeCell.appendChild(gradeBadge);

            // Pass/fail status
            const statusCell = addCell(row, "");
            const statusBadge = document.createElement("span");
            const passed = Number(student.marks) >= 50;

            statusBadge.className =
                "status " + (passed ? "passed" : "failed");
            statusBadge.textContent = passed ? "Passed" : "Failed";
            statusCell.appendChild(statusBadge);

            // Edit and Delete buttons
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

    // Update dashboard statistics.
    function updateDashboard() {
        const total = students.length;

        const passed = students.filter(function (student) {
            return Number(student.marks) >= 50;
        }).length;

        const average = total === 0
            ? 0
            : students.reduce(function (sum, student) {
                return sum + Number(student.marks);
            }, 0) / total;

        const highestMarks = total === 0
            ? null
            : Math.max(...students.map(function (student) {
                return Number(student.marks);
            }));

        document.getElementById("totalStudents").textContent = total;
        document.getElementById("passedStudents").textContent = passed;
        document.getElementById("averageMarks").textContent =
            average.toFixed(1) + "%";

        document.getElementById("topGrade").textContent =
            highestMarks === null ? "-" : calculateGrade(highestMarks);
    }

    // Refresh the table and dashboard.
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

    // Add a new student or update an existing student.
    form.addEventListener("submit", function (event) {
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

        if (!Number.isInteger(Number(rollNo)) || Number(rollNo) <= 0) {
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
            alert("This roll number already exists. Use a different number.");
            return;
        }

        const record = {
            rollNo: rollNo,
            name: name,
            age: age,
            course: course,
            marks: marks
        };

        if (editingRollNo !== null) {
            students = students.map(function (student) {
                return String(student.rollNo) === String(editingRollNo)
                    ? record
                    : student;
            });
        } else {
            students.push(record);
        }

        if (!saveStudents()) {
            return;
        }

        form.reset();
        editingRollNo = null;
        rollInput.readOnly = false;
        formTitle.textContent = "Add New Student";
        saveButton.textContent = "Save Student";

        refreshView();
        alert("Student record saved successfully.");
    });

    // Load a student's information into the form for editing.
    function editStudent(rollNo) {
        const student = students.find(function (item) {
            return String(item.rollNo) === String(rollNo);
        });

        if (!student) return;

        editingRollNo = student.rollNo;

        rollInput.value = student.rollNo;
        nameInput.value = student.name;
        ageInput.value = student.age;
        courseInput.value = student.course;
        marksInput.value = student.marks;

        // Do not change the roll number during editing.
        rollInput.readOnly = true;
        formTitle.textContent = "Edit Student";
        saveButton.textContent = "Update Student";

        document.getElementById("add-student").scrollIntoView({
            behavior: "smooth",
            block: "start"
        });
    }

    // Delete a student record.
    function deleteStudent(rollNo) {
        const student = students.find(function (item) {
            return String(item.rollNo) === String(rollNo);
        });

        if (!student) return;

        const confirmed = confirm(
            "Are you sure you want to delete " + student.name + "?"
        );

        if (!confirmed) return;

        students = students.filter(function (item) {
            return String(item.rollNo) !== String(rollNo);
        });

        if (String(editingRollNo) === String(rollNo)) {
            form.reset();
            editingRollNo = null;
            rollInput.readOnly = false;
            formTitle.textContent = "Add New Student";
            saveButton.textContent = "Save Student";
        }

        if (saveStudents()) {
            refreshView();
        }
    }

    // Search by roll number, student name, or course.
    searchInput.addEventListener("input", refreshView);

    // Reset the form's editing state when Clear Form is clicked.
    form.addEventListener("reset", function () {
        editingRollNo = null;
        rollInput.readOnly = false;
        formTitle.textContent = "Add New Student";
        saveButton.textContent = "Save Student";
    });

    // Show the records when the page opens.
    refreshView();
});
