// UNIQUE FEATURE: Target SGPA Planner (greedy "what-if" analysis)
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

struct Grade {
    std::string letter;
    double points;
};

struct GradeScale {
    static const double PASS_PERCENT;
};

const double GradeScale::PASS_PERCENT = 40.0;

struct SubjectMark {
    static const double MAX_EXTERNAL;

    std::string code;
    std::string name;
    int credits;
    double internal;
    double external;

    double total() const { return internal + external; }

    Grade grade() const {
        static const double thresholds[] = {40, 50, 60, 70, 80, 90};
        static const char* letters[] = {"C", "B", "B+", "A", "A+", "O"};
        for (int i = 5; i >= 0; --i) {
            if (total() >= thresholds[i]) return Grade{letters[i], static_cast<double>(i + 5)};
        }
        return Grade{"F", 0.0};
    }
};

const double SubjectMark::MAX_EXTERNAL = 60.0;

class Student {
public:
    Student(const std::string& name, const std::string& id,
            const std::vector<SubjectMark>& marks)
        : name_(name), id_(id), marks_(marks) {}

    const std::string& name() const { return name_; }
    const std::string& id() const { return id_; }
    const std::vector<SubjectMark>& marks() const { return marks_; }

private:
    std::string name_;
    std::string id_;
    std::vector<SubjectMark> marks_;
};

class ResultManager {
public:
    void add(const Student& student) { students_.push_back(student); }

    const Student* find(const std::string& id) const {
        for (std::size_t i = 0; i < students_.size(); ++i) {
            if (students_[i].id() == id) return &students_[i];
        }
        return NULL;
    }

private:
    std::vector<Student> students_;
};

namespace util {

double readNumber(const std::string& prompt, double minimum, double maximum) {
    double value;
    while (true) {
        std::cout << prompt;
        if (std::cin >> value && std::isfinite(value) && value >= minimum && value <= maximum) {
            return value;
        }
        std::cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int readInt(const std::string& prompt, int minimum, int maximum) {
    int value;
    while (true) {
        std::cout << prompt;
        if (std::cin >> value && value >= minimum && value <= maximum) return value;
        std::cout << "Enter a whole number from " << minimum << " to " << maximum << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

std::string readLine(const std::string& prompt) {
    std::string value;
    std::cout << prompt;
    std::getline(std::cin >> std::ws, value);
    return value;
}

std::string line(char fill, std::size_t count) {
    return std::string(count, fill);
}

}  // namespace util

namespace {
 
struct PlanItem {
    SubjectMark mark;       // internal is known, external holds the REQUIRED marks
};
 
struct Plan {
    std::vector<PlanItem> items;
    double target = 0;
    double achieved = 0;
    bool feasible = false;
};
 
const double THRESHOLDS[] = {40, 50, 60, 70, 80, 90};     // lower bound of grades C ... O
 
double sgpaOf(const std::vector<PlanItem>& items) {
    double pts = 0;
    int cr = 0;
    for (const auto& it : items) { pts += it.mark.credits * it.mark.grade().points; cr += it.mark.credits; }
    return cr ? pts / cr : 0.0;
}
 
// Start from the minimum external marks that PASS every subject, then repeatedly buy the
// cheapest grade upgrade (extra external marks per credit) until the target is reached.
Plan buildPlan(const Student& s, double target) {
    Plan p;
    p.target = target;
    for (const SubjectMark& m : s.marks()) {
        PlanItem it{m};
        it.mark.external = std::max(0.0, GradeScale::PASS_PERCENT - m.internal);
        p.items.push_back(it);
    }
    while (sgpaOf(p.items) + 1e-9 < target) {
        int best = -1;
        double bestCost = 1e9, bestExt = 0;
        for (size_t i = 0; i < p.items.size(); ++i) {
            const SubjectMark& m = p.items[i].mark;
            for (double t : THRESHOLDS) {
                if (t <= m.total()) continue;
                double ext = t - m.internal;
                if (ext > SubjectMark::MAX_EXTERNAL) break;             // unreachable grade
                double cost = (ext - m.external) / m.credits;
                if (cost < bestCost) { bestCost = cost; best = static_cast<int>(i); bestExt = ext; }
                break;                                                  // only the next level
            }
        }
        if (best < 0) break;                                            // nothing left to upgrade
        p.items[static_cast<size_t>(best)].mark.external = bestExt;
    }
    p.achieved = sgpaOf(p.items);
    p.feasible = p.achieved + 1e-9 >= target;
    return p;
}
 
void printPlan(const Student& s, const Plan& p, std::ostream& os) {
    os << std::fixed << std::setprecision(2) << "\nPlan for " << s.name() << " (" << s.id() << ")  Target SGPA: "
       << p.target << "\n" << std::left << std::setw(9) << "Code" << std::setw(34) << "Subject" << std::right
       << std::setw(4) << "Cr" << std::setw(6) << "Int" << std::setw(10) << "ReqExt" << std::setw(8) << "Total"
       << std::setw(5) << "Gr" << "\n" << util::line('-', 76) << "\n" << std::setprecision(0);
    double totalExt = 0;
    for (const auto& it : p.items) {
        const SubjectMark& m = it.mark;
        os << std::left << std::setw(9) << m.code << std::setw(34) << m.name.substr(0, 33) << std::right
           << std::setw(4) << m.credits << std::setw(6) << m.internal << std::setw(8) << m.external << "/60"
           << std::setw(8) << m.total() << std::setw(5) << m.grade().letter << "\n";
        totalExt += m.external;
    }
    os << util::line('-', 76) << std::setprecision(2) << "\nProjected SGPA: " << p.achieved
       << "   Total external marks needed: " << std::setprecision(0) << totalExt << " / "
       << 60 * p.items.size() << "\n";
    if (p.feasible) os << "RESULT: Target is ACHIEVABLE with the marks above.\n";
    else os << "RESULT: Target NOT achievable. Maximum possible SGPA = " << std::setprecision(2) << p.achieved << "\n";
}
 
// Required SGPA in the coming semester to reach a target CGPA.
void cgpaCalculator() {
    double cg = util::readNumber("Current CGPA (0-10): ", 0, 10);
    int done = util::readInt("Credits completed so far: ", 1, 400);
    int next = util::readInt("Credits of the coming semester: ", 1, 40);
    double target = util::readNumber("Target CGPA (0-10): ", 0, 10);
    double need = (target * (done + next) - cg * done) / next;
    std::cout << std::fixed << std::setprecision(2) << "Required SGPA next semester: " << need << "\n";
    if (need > 10) std::cout << "Not possible in one semester - spread the target over more semesters.\n";
    else if (need <= 0) std::cout << "Target already secured.\n";
    else std::cout << "Possible. Aim for at least an average of grade point " << std::ceil(need) << ".\n";
}

void addStudent(ResultManager& rm) {
    const std::string id = util::readLine("Student UID: ");
    if (rm.find(id)) {
        std::cout << "That UID already has a record.\n";
        return;
    }

    const std::string name = util::readLine("Student name: ");
    const int count = util::readInt("Number of subjects (1-30): ", 1, 30);
    std::vector<SubjectMark> marks;
    for (int i = 0; i < count; ++i) {
        std::cout << "\nSubject " << (i + 1) << " of " << count << "\n";
        SubjectMark mark;
        mark.code = util::readLine("Subject code: ");
        mark.name = util::readLine("Subject name: ");
        mark.credits = util::readInt("Credits (1-10): ", 1, 10);
        mark.internal = util::readNumber("Internal marks (0-40): ", 0, 40);
        mark.external = 0;
        marks.push_back(mark);
    }

    rm.add(Student(name, id, marks));
    std::cout << "Student record added.\n";
}
 
}  // namespace
 
void runSpecialFeature(ResultManager& rm) {
    while (true) {
        std::cout << "\n--- TARGET SGPA PLANNER ---\n 1. Add student record\n"
                  << " 2. Plan external marks for a target SGPA\n"
                  << " 3. Required SGPA for a target CGPA\n 0. Exit\n";
        int c = util::readInt("Choice: ", 0, 3);
        if (c == 0) return;
        if (c == 1) { addStudent(rm); continue; }
        if (c == 3) { cgpaCalculator(); continue; }
        const Student* s = rm.find(util::readLine("UID (internal marks are read from the record): "));
        if (!s) {
            std::cout << "Student not found. Add the record first.\n";
            continue;
        }
        double target = util::readNumber("Target SGPA (0-10): ", 0, 10);
        printPlan(*s, buildPlan(*s, target), std::cout);
    }
}

int main() {
    ResultManager resultManager;
    runSpecialFeature(resultManager);
    return 0;
}
