#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr int LOAN_DAYS = 14;
constexpr double FINE_PER_DAY_GBP = 0.20;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string csvEscape(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) return value;
    std::string escaped = "\"";
    for (char ch : value) {
        escaped += (ch == '\"') ? "\"\"" : std::string(1, ch);
    }
    escaped += "\"";
    return escaped;
}

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '\"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '\"') {
                field += '\"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.push_back(field);
    return fields;
}

std::tm localTime(std::time_t time) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &time);
#else
    localtime_r(&time, &result);
#endif
    return result;
}

std::string formatDate(std::time_t time) {
    const std::tm tm = localTime(time);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d");
    return out.str();
}

std::string currentDate() {
    return formatDate(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::optional<std::time_t> parseDate(const std::string& date) {
    std::tm tm{};
    std::istringstream input(date);
    input >> std::get_time(&tm, "%Y-%m-%d");
    if (input.fail()) return std::nullopt;

    tm.tm_hour = 12;
    tm.tm_isdst = -1;
    const std::time_t time = std::mktime(&tm);
    if (time == -1 || formatDate(time) != date) return std::nullopt;
    return time;
}

std::string addDays(const std::string& date, int days) {
    const auto parsed = parseDate(date);
    if (!parsed) throw std::invalid_argument("Invalid date: " + date);

    std::tm tm = localTime(*parsed);
    tm.tm_mday += days;
    tm.tm_isdst = -1;
    return formatDate(std::mktime(&tm));
}

int daysBetween(const std::string& startDate, const std::string& endDate) {
    const auto start = parseDate(startDate);
    const auto end = parseDate(endDate);
    if (!start || !end) throw std::invalid_argument("Unable to compare invalid dates.");
    return static_cast<int>(std::difftime(*end, *start) / (60 * 60 * 24));
}

int readInt(const std::string& prompt, int minimum, int maximum) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        const std::string cleaned = trim(line);

        try {
            std::size_t processed = 0;
            const int value = std::stoi(cleaned, &processed);
            if (processed != cleaned.size()) throw std::invalid_argument("extra characters");
            if (value < minimum || value > maximum) {
                std::cout << "Please enter a value between " << minimum << " and " << maximum << ".\n";
                continue;
            }
            return value;
        } catch (const std::exception&) {
            std::cout << "Invalid input. Please enter a whole number.\n";
        }
    }
}

std::string readNonEmpty(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string value;
        std::getline(std::cin, value);
        value = trim(value);
        if (!value.empty()) return value;
        std::cout << "This field cannot be empty.\n";
    }
}

std::string readDate(const std::string& prompt) {
    while (true) {
        const std::string value = readNonEmpty(prompt);
        if (parseDate(value)) return value;
        std::cout << "Invalid date. Use YYYY-MM-DD (for example 2026-08-20).\n";
    }
}

std::string makeTransactionId(int bookId) {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "TX-" + std::to_string(bookId) + "-" + std::to_string(millis);
}
} // namespace

class Book {
public:
    Book() = default;

    Book(int id, std::string title, std::string author, int year, std::string publisher,
         int totalCopies, int availableCopies, std::string subject)
        : id_(id), title_(std::move(title)), author_(std::move(author)), year_(year),
          publisher_(std::move(publisher)), totalCopies_(totalCopies),
          availableCopies_(availableCopies), subject_(std::move(subject)) {}

    int id() const { return id_; }
    const std::string& title() const { return title_; }
    const std::string& author() const { return author_; }
    int year() const { return year_; }
    const std::string& publisher() const { return publisher_; }
    int totalCopies() const { return totalCopies_; }
    int availableCopies() const { return availableCopies_; }
    const std::string& subject() const { return subject_; }
    void setAvailableCopies(int value) { availableCopies_ = value; }

    void display() const {
        std::cout << "\n----------------------------------------\n"
                  << "Book ID:            " << id_ << '\n'
                  << "Title:              " << title_ << '\n'
                  << "Author(s):          " << author_ << '\n'
                  << "Publication year:   " << year_ << '\n'
                  << "Publisher:          " << publisher_ << '\n'
                  << "Total copies:       " << totalCopies_ << '\n'
                  << "Available copies:   " << availableCopies_ << '\n'
                  << "Subject:            " << subject_ << '\n'
                  << "----------------------------------------\n";
    }

private:
    int id_ = 0;
    std::string title_;
    std::string author_;
    int year_ = 0;
    std::string publisher_;
    int totalCopies_ = 0;
    int availableCopies_ = 0;
    std::string subject_;
};

struct BorrowRecord {
    std::string transactionId;
    int bookId = 0;
    std::string title;
    std::string borrower;
    std::string borrowDate;
    std::string dueDate;
};

struct ReturnRecord {
    std::string transactionId;
    int bookId = 0;
    std::string title;
    std::string borrower;
    std::string borrowDate;
    std::string returnDate;
    int lateDays = 0;
    double fine = 0.0;
};

class LibrarySystem {
public:
    explicit LibrarySystem(fs::path dataDirectory = "data")
        : dataDirectory_(std::move(dataDirectory)),
          booksFile_(dataDirectory_ / "books.csv"),
          borrowingFile_(dataDirectory_ / "borrowing_records.csv"),
          returnFile_(dataDirectory_ / "return_records.csv"),
          finesFile_(dataDirectory_ / "fines.csv") {
        initialiseDataFiles();
        loadBooks();
    }

    void run() {
        std::cout << "\n=== Stepwise University Library Management System ===\n"
                  << "C++ console application | CSV persistence | Role-based menus\n";

        while (true) {
            std::cout << "\nSelect user type:\n1. Librarian\n2. Student\n0. Exit\n";
            const int role = readInt("Choice: ", 0, 2);
            if (role == 0) {
                std::cout << "Goodbye.\n";
                return;
            }
            role == 1 ? librarianMenu() : studentMenu();
        }
    }

private:
    fs::path dataDirectory_;
    fs::path booksFile_;
    fs::path borrowingFile_;
    fs::path returnFile_;
    fs::path finesFile_;
    std::vector<Book> books_;

    static void ensureFileWithHeader(const fs::path& path, const std::string& header) {
        if (fs::exists(path) && fs::file_size(path) > 0) return;
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Unable to create file: " + path.string());
        out << header << '\n';
    }

    void initialiseDataFiles() {
        fs::create_directories(dataDirectory_);
        ensureFileWithHeader(booksFile_, "book_id,title,author,year,publisher,total_copies,available_copies,subject");
        ensureFileWithHeader(borrowingFile_, "transaction_id,book_id,title,borrower,borrow_date,due_date");
        ensureFileWithHeader(returnFile_, "transaction_id,book_id,title,borrower,borrow_date,return_date,late_days,fine_gbp");
        ensureFileWithHeader(finesFile_, "transaction_id,book_id,borrower,late_days,fine_gbp,recorded_date");
    }

    void loadBooks() {
        books_.clear();
        std::ifstream in(booksFile_);
        if (!in) throw std::runtime_error("Unable to open books file: " + booksFile_.string());

        std::string line;
        std::getline(in, line);
        while (std::getline(in, line)) {
            if (trim(line).empty()) continue;
            const auto fields = parseCsvLine(line);
            if (fields.size() != 8) {
                std::cerr << "Warning: skipping malformed book row.\n";
                continue;
            }

            try {
                books_.emplace_back(std::stoi(fields[0]), fields[1], fields[2], std::stoi(fields[3]),
                                    fields[4], std::stoi(fields[5]), std::stoi(fields[6]), fields[7]);
            } catch (const std::exception&) {
                std::cerr << "Warning: skipping book row containing invalid numeric data.\n";
            }
        }
    }

    void saveBooks() const {
        std::ofstream out(booksFile_, std::ios::trunc);
        if (!out) throw std::runtime_error("Unable to save books file.");

        out << "book_id,title,author,year,publisher,total_copies,available_copies,subject\n";
        for (const Book& book : books_) {
            out << book.id() << ',' << csvEscape(book.title()) << ',' << csvEscape(book.author()) << ','
                << book.year() << ',' << csvEscape(book.publisher()) << ',' << book.totalCopies() << ','
                << book.availableCopies() << ',' << csvEscape(book.subject()) << '\n';
        }
    }

    Book* findBookById(int bookId) {
        auto it = std::find_if(books_.begin(), books_.end(), [bookId](const Book& book) {
            return book.id() == bookId;
        });
        return it == books_.end() ? nullptr : &(*it);
    }

    const Book* findBookById(int bookId) const {
        auto it = std::find_if(books_.begin(), books_.end(), [bookId](const Book& book) {
            return book.id() == bookId;
        });
        return it == books_.end() ? nullptr : &(*it);
    }

    void registerBook() {
        std::cout << "\n=== Register a new book ===\n";
        const int bookId = readInt("Book ID: ", 1, 999999999);
        if (findBookById(bookId)) {
            std::cout << "A book with that ID already exists.\n";
            return;
        }

        const std::string title = readNonEmpty("Title: ");
        const std::string author = readNonEmpty("Author(s): ");
        const int year = readInt("Publication year: ", 1000, 2100);
        const std::string publisher = readNonEmpty("Publisher: ");
        const int totalCopies = readInt("Total number of copies: ", 1, 100000);
        const int availableCopies = readInt("Currently available copies: ", 0, totalCopies);
        const std::string subject = readNonEmpty("Subject: ");

        books_.emplace_back(bookId, title, author, year, publisher, totalCopies, availableCopies, subject);
        saveBooks();
        std::cout << "Book registered and saved successfully.\n";
    }

    void searchBooks() const {
        std::cout << "\n=== Search catalogue ===\n1. Search by Book ID\n2. Search by title (case-insensitive)\n";
        const int option = readInt("Choice: ", 1, 2);

        if (option == 1) {
            const int bookId = readInt("Book ID: ", 1, 999999999);
            const Book* book = findBookById(bookId);
            if (book) book->display();
            else std::cout << "No book found with ID " << bookId << ".\n";
            return;
        }

        const std::string query = toLower(readNonEmpty("Title or part of title: "));
        bool found = false;
        for (const Book& book : books_) {
            if (toLower(book.title()).find(query) != std::string::npos) {
                book.display();
                found = true;
            }
        }
        if (!found) std::cout << "No matching books found.\n";
    }

    void borrowBook() {
        std::cout << "\n=== Borrow a book ===\n";
        const int bookId = readInt("Book ID: ", 1, 999999999);
        Book* book = findBookById(bookId);
        if (!book) {
            std::cout << "Book not found.\n";
            return;
        }
        if (book->availableCopies() <= 0) {
            std::cout << "No copies are currently available.\n";
            return;
        }

        const std::string borrower = readNonEmpty("Borrower name or student/staff ID: ");
        const std::string borrowDate = currentDate();
        const std::string dueDate = addDays(borrowDate, LOAN_DAYS);
        const std::string transactionId = makeTransactionId(bookId);

        std::ofstream out(borrowingFile_, std::ios::app);
        if (!out) throw std::runtime_error("Unable to write borrowing record.");
        out << transactionId << ',' << bookId << ',' << csvEscape(book->title()) << ','
            << csvEscape(borrower) << ',' << borrowDate << ',' << dueDate << '\n';

        book->setAvailableCopies(book->availableCopies() - 1);
        saveBooks();

        std::cout << "Borrowing recorded successfully.\n"
                  << "Transaction: " << transactionId << '\n'
                  << "Due date:    " << dueDate << '\n';
    }

    std::vector<BorrowRecord> loadBorrowings() const {
        std::vector<BorrowRecord> records;
        std::ifstream in(borrowingFile_);
        if (!in) return records;

        std::string line;
        std::getline(in, line);
        while (std::getline(in, line)) {
            if (trim(line).empty()) continue;
            const auto fields = parseCsvLine(line);
            if (fields.size() != 6) continue;
            try {
                records.push_back({fields[0], std::stoi(fields[1]), fields[2], fields[3], fields[4], fields[5]});
            } catch (const std::exception&) {
            }
        }
        return records;
    }

    std::vector<ReturnRecord> loadReturns() const {
        std::vector<ReturnRecord> records;
        std::ifstream in(returnFile_);
        if (!in) return records;

        std::string line;
        std::getline(in, line);
        while (std::getline(in, line)) {
            if (trim(line).empty()) continue;
            const auto fields = parseCsvLine(line);
            if (fields.size() != 8) continue;
            try {
                records.push_back({fields[0], std::stoi(fields[1]), fields[2], fields[3], fields[4], fields[5],
                                   std::stoi(fields[6]), std::stod(fields[7])});
            } catch (const std::exception&) {
            }
        }
        return records;
    }

    std::set<std::string> returnedTransactionIds() const {
        std::set<std::string> ids;
        for (const ReturnRecord& record : loadReturns()) ids.insert(record.transactionId);
        return ids;
    }

    void returnBook() {
        std::cout << "\n=== Return a book ===\n";
        const int bookId = readInt("Book ID: ", 1, 999999999);
        Book* book = findBookById(bookId);
        if (!book) {
            std::cout << "Book not found.\n";
            return;
        }

        const std::string borrower = readNonEmpty("Borrower name or student/staff ID: ");
        const std::string borrowerKey = toLower(borrower);
        const auto borrowings = loadBorrowings();
        const auto returned = returnedTransactionIds();

        const BorrowRecord* active = nullptr;
        for (auto it = borrowings.rbegin(); it != borrowings.rend(); ++it) {
            if (it->bookId == bookId && toLower(it->borrower) == borrowerKey &&
                returned.find(it->transactionId) == returned.end()) {
                active = &(*it);
                break;
            }
        }

        if (!active) {
            std::cout << "No active borrowing record was found for that book and borrower.\n";
            return;
        }

        const std::string returnDate = readDate("Return date (YYYY-MM-DD, use today's date when appropriate): ");
        if (daysBetween(active->borrowDate, returnDate) < 0) {
            std::cout << "Return date cannot be before the borrowing date (" << active->borrowDate << ").\n";
            return;
        }

        const int lateDays = std::max(0, daysBetween(active->dueDate, returnDate));
        const double fine = lateDays * FINE_PER_DAY_GBP;

        std::ofstream returnOut(returnFile_, std::ios::app);
        if (!returnOut) throw std::runtime_error("Unable to write return record.");
        returnOut << active->transactionId << ',' << active->bookId << ',' << csvEscape(active->title) << ','
                  << csvEscape(active->borrower) << ',' << active->borrowDate << ',' << returnDate << ','
                  << lateDays << ',' << std::fixed << std::setprecision(2) << fine << '\n';

        if (fine > 0.0) {
            std::ofstream fineOut(finesFile_, std::ios::app);
            if (!fineOut) throw std::runtime_error("Unable to write fine record.");
            fineOut << active->transactionId << ',' << active->bookId << ',' << csvEscape(active->borrower) << ','
                    << lateDays << ',' << std::fixed << std::setprecision(2) << fine << ',' << returnDate << '\n';
        }

        if (book->availableCopies() < book->totalCopies()) {
            book->setAvailableCopies(book->availableCopies() + 1);
            saveBooks();
        }

        std::cout << "Return recorded successfully.\n"
                  << "Late days: " << lateDays << '\n'
                  << "Fine:      GBP " << std::fixed << std::setprecision(2) << fine << '\n';
    }

    void searchBorrowingRecords() const {
        const auto records = loadBorrowings();
        if (records.empty()) {
            std::cout << "No borrowing records found.\n";
            return;
        }

        std::cout << "\nSearch borrowing records by:\n1. Book ID\n2. Borrower\n";
        const int option = readInt("Choice: ", 1, 2);
        bool found = false;

        if (option == 1) {
            const int bookId = readInt("Book ID: ", 1, 999999999);
            for (const auto& record : records) {
                if (record.bookId == bookId) {
                    displayBorrow(record);
                    found = true;
                }
            }
        } else {
            const std::string query = toLower(readNonEmpty("Borrower search: "));
            for (const auto& record : records) {
                if (toLower(record.borrower).find(query) != std::string::npos) {
                    displayBorrow(record);
                    found = true;
                }
            }
        }
        if (!found) std::cout << "No matching borrowing records found.\n";
    }

    void searchReturnRecords() const {
        const auto records = loadReturns();
        if (records.empty()) {
            std::cout << "No return records found.\n";
            return;
        }

        std::cout << "\nSearch return records by:\n1. Book ID\n2. Borrower\n";
        const int option = readInt("Choice: ", 1, 2);
        bool found = false;

        if (option == 1) {
            const int bookId = readInt("Book ID: ", 1, 999999999);
            for (const auto& record : records) {
                if (record.bookId == bookId) {
                    displayReturn(record);
                    found = true;
                }
            }
        } else {
            const std::string query = toLower(readNonEmpty("Borrower search: "));
            for (const auto& record : records) {
                if (toLower(record.borrower).find(query) != std::string::npos) {
                    displayReturn(record);
                    found = true;
                }
            }
        }
        if (!found) std::cout << "No matching return records found.\n";
    }

    void searchFines() const {
        std::ifstream in(finesFile_);
        if (!in) {
            std::cout << "No fine records found.\n";
            return;
        }

        std::cout << "\nSearch fines by:\n1. Book ID\n2. Borrower\n";
        const int option = readInt("Choice: ", 1, 2);
        const int bookIdQuery = option == 1 ? readInt("Book ID: ", 1, 999999999) : -1;
        const std::string borrowerQuery = option == 2 ? toLower(readNonEmpty("Borrower search: ")) : "";

        std::string line;
        std::getline(in, line);
        bool found = false;
        while (std::getline(in, line)) {
            const auto fields = parseCsvLine(line);
            if (fields.size() != 6) continue;
            try {
                const int bookId = std::stoi(fields[1]);
                const bool matches = option == 1 ? bookId == bookIdQuery
                                                 : toLower(fields[2]).find(borrowerQuery) != std::string::npos;
                if (matches) {
                    std::cout << "\nTransaction: " << fields[0] << '\n'
                              << "Book ID:     " << fields[1] << '\n'
                              << "Borrower:    " << fields[2] << '\n'
                              << "Late days:   " << fields[3] << '\n'
                              << "Fine:        GBP " << fields[4] << '\n'
                              << "Recorded:    " << fields[5] << '\n';
                    found = true;
                }
            } catch (const std::exception&) {
            }
        }
        if (!found) std::cout << "No matching fine records found.\n";
    }

    static void displayBorrow(const BorrowRecord& record) {
        std::cout << "\nTransaction: " << record.transactionId << '\n'
                  << "Book ID:     " << record.bookId << '\n'
                  << "Title:       " << record.title << '\n'
                  << "Borrower:    " << record.borrower << '\n'
                  << "Borrowed:    " << record.borrowDate << '\n'
                  << "Due:         " << record.dueDate << '\n';
    }

    static void displayReturn(const ReturnRecord& record) {
        std::cout << "\nTransaction: " << record.transactionId << '\n'
                  << "Book ID:     " << record.bookId << '\n'
                  << "Title:       " << record.title << '\n'
                  << "Borrower:    " << record.borrower << '\n'
                  << "Borrowed:    " << record.borrowDate << '\n'
                  << "Returned:    " << record.returnDate << '\n'
                  << "Late days:   " << record.lateDays << '\n'
                  << "Fine:        GBP " << std::fixed << std::setprecision(2) << record.fine << '\n';
    }

    void librarianMenu() {
        while (true) {
            std::cout << "\n=== Librarian Menu ===\n"
                      << "1. Register a new book\n2. Search catalogue\n3. Borrow a book\n4. Return a book\n"
                      << "5. Search borrowing records\n6. Search return records\n7. Search fines\n0. Back\n";
            const int choice = readInt("Choice: ", 0, 7);
            try {
                switch (choice) {
                    case 0: return;
                    case 1: registerBook(); break;
                    case 2: searchBooks(); break;
                    case 3: borrowBook(); break;
                    case 4: returnBook(); break;
                    case 5: searchBorrowingRecords(); break;
                    case 6: searchReturnRecords(); break;
                    case 7: searchFines(); break;
                }
            } catch (const std::exception& ex) {
                std::cerr << "Operation failed: " << ex.what() << '\n';
            }
        }
    }

    void studentMenu() {
        while (true) {
            std::cout << "\n=== Student Menu ===\n"
                      << "1. Search catalogue\n2. Borrow a book\n3. Return a book\n"
                      << "4. Search borrowing records\n5. Search return records\n6. Search fines\n0. Back\n";
            const int choice = readInt("Choice: ", 0, 6);
            try {
                switch (choice) {
                    case 0: return;
                    case 1: searchBooks(); break;
                    case 2: borrowBook(); break;
                    case 3: returnBook(); break;
                    case 4: searchBorrowingRecords(); break;
                    case 5: searchReturnRecords(); break;
                    case 6: searchFines(); break;
                }
            } catch (const std::exception& ex) {
                std::cerr << "Operation failed: " << ex.what() << '\n';
            }
        }
    }
};

int main() {
    try {
        LibrarySystem system;
        system.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
