#pragma once
// DataStore: saves and loads all RentalData to ONE versioned text file.
//
// FILE FORMAT (version 1) - one record per line, fields separated by TAB:
//
//   SVRMS-DATA <TAB> 1                 header + format version (first line)
//   MODEL <TAB> M-0001 <TAB> Maruti ... one line per record
//   ...
//   END <TAB> <number of records>      footer (detects truncated files)
//
// Text fields are escaped so names/notes may contain anything:
//   backslash -> \\    TAB -> \t    newline -> \n    carriage return -> \r
// Money is stored as integer paise, factors as integer basis points, dates as
// YYYY-MM-DD. Derived vehicle types are rebuilt from the category field
// through makeVehicle().
//
// SAFE SAVING: write everything to "<file>.tmp", read it back to prove it
// loads, then rename the old file to "<file>.bak" and the temp file to
// "<file>". A crash can lose at most the last change; the .bak holds the
// previous good version. (Rename is atomic on POSIX; on Windows each rename is
// a separate step, which is why the .bak is kept.)
//
// SINGLE PROCESS: there is no file locking. Run one copy of the app per data
// file; two copies would overwrite each other's changes.

#include "rental/RentalData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace rental {

class DataStore {
public:
    static constexpr int kFormatVersion = 1;

    explicit DataStore(std::filesystem::path file) : file_(std::move(file)) {}

    const std::filesystem::path& path() const { return file_; }
    std::filesystem::path backupPath() const { return std::filesystem::path(file_.string() + ".bak"); }
    std::filesystem::path tempPath() const { return std::filesystem::path(file_.string() + ".tmp"); }
    bool exists() const { return std::filesystem::exists(file_); }

    // Throws PersistenceError with the line number if anything is wrong.
    // Never modifies the file.
    RentalData load() const;
    void save(const RentalData& data) const;

    // Exposed for tests and for anyone curious about the format.
    static std::string serialize(const RentalData& data);
    static RentalData deserialize(const std::string& content);
    static std::string escapeField(const std::string& raw);
    static std::string unescapeField(const std::string& escaped);
    static std::vector<std::string> splitRecord(const std::string& line);

private:
    std::filesystem::path file_;
};

} // namespace rental
