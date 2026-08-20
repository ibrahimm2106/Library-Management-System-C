# Testing Guide

This repository includes a practical manual test plan for the main functional and boundary cases in the library management system.

## Build test

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

Expected result: the project compiles without errors using a C++17 compiler.

## Functional test cases

| ID | Scenario | Steps | Expected result |
|---|---|---|---|
| T01 | Role selection validation | Enter text, then `3`, then `1` | Invalid values are rejected; Librarian menu opens after valid input. |
| T02 | Register a book | Librarian → Register; enter a unique ID and valid details | Book is appended to the catalogue and persisted to `data/books.csv`. |
| T03 | Duplicate Book ID | Register a second book with the same ID | Operation is rejected without changing the file. |
| T04 | Case-insensitive title search | Search for `clean code`, `CLEAN CODE`, or part of the title | The same matching book is displayed. |
| T05 | Borrow available book | Borrow a book with available copies | Available count decreases by one and a borrowing record is written with a 14-day due date. |
| T06 | Borrow unavailable book | Borrow until available copies reach zero, then borrow again | Borrowing is rejected and inventory does not become negative. |
| T07 | Return active loan | Return using the same Book ID and borrower reference | Return record is written and available count increases by one. |
| T08 | Return without active loan | Attempt to return a book that the borrower has not borrowed | Operation is rejected with a clear message. |
| T09 | On-time return | Return on/before due date | Fine is `GBP 0.00`; no fine row is added. |
| T10 | Late return | Return 5 days after due date | Fine is `5 × £0.20 = GBP 1.00` and is persisted to `data/fines.csv`. |
| T11 | Invalid date | Enter an impossible date such as `2026-02-31` | Date is rejected and user is asked again. |
| T12 | Record search | Search borrowing/return/fine records by Book ID or borrower | Matching records are displayed; no-match case is handled clearly. |

## Robustness checks

- Empty text fields are rejected.
- Numeric input is range checked and non-numeric input does not terminate the program.
- CSV rows with malformed external data are skipped rather than crashing the application.
- File open/write failures are surfaced as exceptions and shown as user-readable errors.
- A return date earlier than the recorded borrowing date is rejected.
- Available copies never exceed total copies during a valid return.

## CI

`.github/workflows/build.yml` builds the project on both Ubuntu and Windows for pushes and pull requests to `main`. This provides a quick cross-platform compile check in addition to local testing.
