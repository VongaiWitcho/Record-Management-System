# 📘 Record Management System (C++ + MySQL ODBC)

This project is a simple **Record Management System** implemented in **C++** that connects to a **MySQL database** using the **ODBC (Unicode) connector**.  
It demonstrates how to perform basic CRUD operations (Create, Read, Update, Delete), sorting, and exporting to CSV via a command-line menu system.

---

## ⚙️ Features
- Add new records (Book: `ID`, `Title`, `Author`, `Year`)  
- Display all records  
- Search record by ID  
- Update an existing record  
- Delete record  
- Sort records (by Title)  
- Export records to CSV file  

![Screenshot1](org1.PNG)
![Screenshot2](org2.PNG)
![Screenshot3](org3.PNG)
![Screenshot3](org3.PNG)
---

## 🛠️ Requirements
- Visual Studio (x64 build)  
- MySQL Server (running locally or remotely)  
- MySQL ODBC 9.4 Unicode Driver (configured via **ODBC Data Source Administrator**)  
- DSN configured as `record_db_dsn` pointing to your MySQL database `record_db`

**Database schema:**
```sql
CREATE DATABASE record_db;
USE record_db;

CREATE TABLE books (
    id INT PRIMARY KEY,
    title VARCHAR(255),
    author VARCHAR(255),
    year INT
);
