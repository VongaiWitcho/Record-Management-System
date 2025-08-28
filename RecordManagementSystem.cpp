#include <windows.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <io.h>
#include <fcntl.h>
#include <limits>
#include <cwchar>

// Avoid Windows macro clash with std::numeric_limits<>::max()
#undef max

using namespace std;

// Data Model
struct Book {
    int id;
    wstring title;
    wstring author;
    int year;
};

// Global ODBC handles
SQLHENV hEnv{};
SQLHDBC hDbc{};

// Utility: Print ODBC errors (Unicode)
void showError(SQLSMALLINT handleType, SQLHANDLE handle) {
    SQLWCHAR sqlState[6]{}, message[256]{};
    SQLINTEGER nativeError{};
    SQLSMALLINT msgLen{};
    SQLRETURN ret;
    SQLSMALLINT i = 1;

    while ((ret = SQLGetDiagRecW(
        handleType,
        handle,
        i,
        sqlState,
        &nativeError,
        message,
        static_cast<SQLSMALLINT>(sizeof(message) / sizeof(SQLWCHAR)), // length in characters
        &msgLen)) != SQL_NO_DATA) {
        wcout << L"ODBC Error [" << sqlState << L"] : " << message << endl;
        ++i;
    }
}

// Initialize ODBC connection
bool connectDB() {
    SQLRETURN ret;
    const SQLWCHAR connStr[] = L"DSN=record_db_dsn;UID=root;PWD=root;";
    SQLWCHAR outConnStr[1024]{};
    SQLSMALLINT outConnStrLen{};

    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

    ret = SQLDriverConnectW(
        hDbc,
        NULL,
        (SQLWCHAR*)connStr,
        SQL_NTS,
        outConnStr,
        static_cast<SQLSMALLINT>(sizeof(outConnStr) / sizeof(SQLWCHAR)),
        &outConnStrLen,
        SQL_DRIVER_COMPLETE
    );

    if (SQL_SUCCEEDED(ret)) {
        wcout << L"✅ ODBC Connection established successfully!\n";
        return true;
    }
    else {
        wcout << L"❌ Connection Failed via ODBC.\n";
        showError(SQL_HANDLE_DBC, hDbc);
        return false;
    }
}

// Cleanup
void disconnectDB() {
    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

// Fetch all records into a vector
vector<Book> fetchAllRecords() {
    vector<Book> records;
    SQLHSTMT hStmt{};
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    const SQLWCHAR query[] = L"SELECT id, title, author, year FROM books";
    if (SQL_SUCCEEDED(SQLExecDirectW(hStmt, (SQLWCHAR*)query, SQL_NTS))) {
        SQLINTEGER id{}, year{};
        SQLWCHAR title[256]{}, author[256]{};
        SQLLEN idInd{}, titleInd{}, authorInd{}, yearInd{};

        while (SQLFetch(hStmt) == SQL_SUCCESS) {
            SQLGetData(hStmt, 1, SQL_C_LONG, &id, 0, &idInd);
            // ✅ Use SQLGetData (wide via SQL_C_WCHAR), not SQLGetDataW
            SQLGetData(hStmt, 2, SQL_C_WCHAR, title, sizeof(title), &titleInd);
            SQLGetData(hStmt, 3, SQL_C_WCHAR, author, sizeof(author), &authorInd);
            SQLGetData(hStmt, 4, SQL_C_LONG, &year, 0, &yearInd);

            records.push_back({ static_cast<int>(id), wstring(title), wstring(author), static_cast<int>(year) });
        }
    }
    else {
        showError(SQL_HANDLE_STMT, hStmt);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return records;
}

// Add a new record
void addRecord() {
    Book b;
    wcout << L"\n--- Add New Book Record ---\n";
    wcout << L"Enter ID: "; wcin >> b.id;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }
    wcin.ignore(numeric_limits<streamsize>::max(), L'\n');
    wcout << L"Enter Title: "; getline(wcin, b.title);
    wcout << L"Enter Author: "; getline(wcin, b.author);
    wcout << L"Enter Year: "; wcin >> b.year;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }

    SQLHSTMT hStmt{};
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    const SQLWCHAR query[] = L"INSERT INTO books (id, title, author, year) VALUES (?, ?, ?, ?)";
    SQLPrepareW(hStmt, (SQLWCHAR*)query, SQL_NTS);

    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &b.id, 0, NULL);
    SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 255, 0, (SQLWCHAR*)b.title.c_str(), 0, NULL);
    SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 255, 0, (SQLWCHAR*)b.author.c_str(), 0, NULL);
    SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &b.year, 0, NULL);

    if (SQL_SUCCEEDED(SQLExecute(hStmt))) {
        wcout << L"✅ Record added successfully!\n";
    }
    else {
        wcout << L"❌ Failed to add record.\n";
        showError(SQL_HANDLE_STMT, hStmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

// Display all records
void displayAllRecords() {
    vector<Book> books = fetchAllRecords();
    if (books.empty()) {
        wcout << L"No records found.\n";
        return;
    }

    wcout << L"\n--- All Book Records ---\n";
    wcout << left << setw(5) << L"ID" << setw(40) << L"Title" << setw(30) << L"Author" << setw(5) << L"Year" << endl;
    wcout << wstring(85, L'-') << endl;
    for (const auto& book : books) {
        wcout << left << setw(5) << book.id
            << setw(40) << book.title
            << setw(30) << book.author
            << setw(5) << book.year << endl;
    }
}

// Search record by ID
void searchRecordByID() {
    int searchID;
    wcout << L"\n--- Search for Book Record ---\n";
    wcout << L"Enter ID to search: "; wcin >> searchID;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }

    SQLHSTMT hStmt{};
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    const SQLWCHAR query[] = L"SELECT id, title, author, year FROM books WHERE id = ?";
    SQLPrepareW(hStmt, (SQLWCHAR*)query, SQL_NTS);
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &searchID, 0, NULL);

    if (SQL_SUCCEEDED(SQLExecute(hStmt))) {
        SQLINTEGER id{}, year{};
        SQLWCHAR title[256]{}, author[256]{};
        SQLLEN idInd{}, titleInd{}, authorInd{}, yearInd{};

        SQLBindCol(hStmt, 1, SQL_C_LONG, &id, 0, &idInd);
        SQLBindCol(hStmt, 2, SQL_C_WCHAR, title, sizeof(title), &titleInd);
        SQLBindCol(hStmt, 3, SQL_C_WCHAR, author, sizeof(author), &authorInd);
        SQLBindCol(hStmt, 4, SQL_C_LONG, &year, 0, &yearInd);

        if (SQLFetch(hStmt) == SQL_SUCCESS) {
            wcout << L"\n--- Record Found! ---\n";
            wcout << L"ID: " << id << L" | Title: " << title << L" | Author: " << author << L" | Year: " << year << endl;
        }
        else {
            wcout << L"❌ Record not found.\n";
        }
    }
    else {
        showError(SQL_HANDLE_STMT, hStmt);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

// Delete record by ID
void deleteRecord() {
    int id;
    wcout << L"\n--- Delete Book Record ---\n";
    wcout << L"Enter ID of the record to delete: "; wcin >> id;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }

    SQLHSTMT hStmt{};
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    const SQLWCHAR query[] = L"DELETE FROM books WHERE id = ?";
    SQLPrepareW(hStmt, (SQLWCHAR*)query, SQL_NTS);
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, NULL);

    if (SQL_SUCCEEDED(SQLExecute(hStmt))) {
        SQLLEN rowsAffected{};
        SQLRowCount(hStmt, &rowsAffected);
        if (rowsAffected > 0) {
            wcout << L"✅ Record with ID " << id << L" deleted successfully.\n";
        }
        else {
            wcout << L"❌ Record with ID " << id << L" not found.\n";
        }
    }
    else {
        showError(SQL_HANDLE_STMT, hStmt);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

// Update record by ID
void updateRecord() {
    int id;
    wcout << L"\n--- Update Book Record ---\n";
    wcout << L"Enter ID of the record to update: "; wcin >> id;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }
    wcin.ignore(numeric_limits<streamsize>::max(), L'\n');

    // Check existence
    SQLHSTMT hStmtCheck{};
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmtCheck);
    const SQLWCHAR checkQuery[] = L"SELECT id FROM books WHERE id = ?";
    SQLPrepareW(hStmtCheck, (SQLWCHAR*)checkQuery, SQL_NTS);
    SQLBindParameter(hStmtCheck, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, NULL);

    if (SQL_SUCCEEDED(SQLExecute(hStmtCheck)) && SQLFetch(hStmtCheck) == SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtCheck);

        wstring newTitle, newAuthor;
        int newYear{};

        wcout << L"Enter new Title: "; getline(wcin, newTitle);
        wcout << L"Enter new Author: "; getline(wcin, newAuthor);
        wcout << L"Enter new Year: "; wcin >> newYear;
        if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }

        SQLHSTMT hStmtUpdate{};
        SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmtUpdate);
        const SQLWCHAR updateQuery[] = L"UPDATE books SET title = ?, author = ?, year = ? WHERE id = ?";
        SQLPrepareW(hStmtUpdate, (SQLWCHAR*)updateQuery, SQL_NTS);

        SQLBindParameter(hStmtUpdate, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 255, 0, (SQLWCHAR*)newTitle.c_str(), 0, NULL);
        SQLBindParameter(hStmtUpdate, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 255, 0, (SQLWCHAR*)newAuthor.c_str(), 0, NULL);
        SQLBindParameter(hStmtUpdate, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &newYear, 0, NULL);
        SQLBindParameter(hStmtUpdate, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, NULL);

        if (SQL_SUCCEEDED(SQLExecute(hStmtUpdate))) {
            wcout << L"✅ Record with ID " << id << L" updated successfully.\n";
        }
        else {
            showError(SQL_HANDLE_STMT, hStmtUpdate);
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtUpdate);

    }
    else {
        wcout << L"❌ Record with ID " << id << L" not found.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtCheck);
    }
}

// Sort records
void sortRecords() {
    vector<Book> books = fetchAllRecords();
    if (books.empty()) {
        wcout << L"No records to sort.\n";
        return;
    }

    int choice{};
    wcout << L"\n--- Sort Records ---\n";
    wcout << L"1. Sort by Title (A-Z)\n";
    wcout << L"2. Sort by Year (Ascending)\n";
    wcout << L"Enter choice: "; wcin >> choice;
    if (wcin.fail()) { wcout << L"Invalid input. Please enter a number.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); return; }

    switch (choice) {
    case 1:
        sort(books.begin(), books.end(), [](const Book& a, const Book& b) { return a.title < b.title; });
        break;
    case 2:
        sort(books.begin(), books.end(), [](const Book& a, const Book& b) { return a.year < b.year; });
        break;
    default:
        wcout << L"Invalid sort choice. Displaying unsorted.\n";
        break;
    }

    wcout << L"\n--- Sorted Book Records ---\n";
    wcout << left << setw(5) << L"ID" << setw(40) << L"Title" << setw(30) << L"Author" << setw(5) << L"Year" << endl;
    wcout << wstring(85, L'-') << endl;
    for (const auto& book : books) {
        wcout << left << setw(5) << book.id
            << setw(40) << book.title
            << setw(30) << book.author
            << setw(5) << book.year << endl;
    }
}

// Export to CSV
void exportToCSV() {
    vector<Book> books = fetchAllRecords();
    if (books.empty()) {
        wcout << L"No records to export.\n";
        return;
    }

    // UTF-16 CSV (works with Excel on Windows)
    wofstream outFile(L"books.csv", ios::binary);
    if (!outFile.is_open()) {
        wcout << L"❌ Error: Could not create CSV file.\n";
        return;
    }

    // Write BOM for UTF-16LE
    wchar_t bom = 0xFEFF;
    outFile.write(&bom, 1);

    outFile << L"ID,Title,Author,Year\n";
    for (const auto& book : books) {
        outFile << book.id << L",\"" << book.title << L"\",\"" << book.author << L"\"," << book.year << L"\n";
    }

    outFile.close();
    wcout << L"✅ All records successfully exported to books.csv.\n";
}

int main() {
    // Enable Unicode for console I/O
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    if (!connectDB()) return 1;

    int choice{};
    do {
        wcout << L"\n===== Record Management System (ODBC Unicode) =====\n";
        wcout << L"1. Add Record\n";
        wcout << L"2. Display All Records\n";
        wcout << L"3. Search Record by ID\n";
        wcout << L"4. Delete Record by ID\n";
        wcout << L"5. Update Record by ID\n";
        wcout << L"6. Sort Records\n";
        wcout << L"7. Export to CSV\n";
        wcout << L"8. Exit\n";
        wcout << L"Enter choice: ";
        wcin >> choice;
        if (wcin.fail()) { wcout << L"Invalid input.\n"; wcin.clear(); wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); continue; }
        wcin.ignore(numeric_limits<streamsize>::max(), L'\n');

        switch (choice) {
        case 1: addRecord(); break;
        case 2: displayAllRecords(); break;
        case 3: searchRecordByID(); break;
        case 4: deleteRecord(); break;
        case 5: updateRecord(); break;
        case 6: sortRecords(); break;
        case 7: exportToCSV(); break;
        case 8: wcout << L"Exiting...\n"; break;
        default: wcout << L"Invalid choice.\n"; break;
        }
    } while (choice != 8);

    disconnectDB();
    return 0;
}
