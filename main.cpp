#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <limits>
#include <list>

using namespace std;

enum class Status {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

Status parseStatus(const string &s) {
    if (s == "Accepted") return Status::Accepted;
    if (s == "Wrong_Answer") return Status::Wrong_Answer;
    if (s == "Runtime_Error") return Status::Runtime_Error;
    if (s == "Time_Limit_Exceed") return Status::Time_Limit_Exceed;
    assert(false);
    return Status::Wrong_Answer;
}

string statusToString(Status s) {
    switch (s) {
        case Status::Accepted: return "Accepted";
        case Status::Wrong_Answer: return "Wrong_Answer";
        case Status::Runtime_Error: return "Runtime_Error";
        case Status::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "";
}

struct Submission {
    char problem;
    Status status;
    int time;
    Submission(char p, Status s, int t) : problem(p), status(s), time(t) {}
};

struct ProblemState {
    int wrong_attempts_before = 0;
    int submissions_after = 0;
    bool solved = false;
    bool frozen = false;
    int solve_time = 0;
    bool solved_before_freeze = false;

    bool countsForRanking() const {
        if (solved_before_freeze) return true;
        if (frozen) return false;
        return solved;
    }
};

class Team {
public:
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;
    int solved_count = 0;
    int total_penalty = 0;
    vector<int> sorted_solve_times;

    Team(const string &n) : name(n) {}

    void addProblem(char p) {
        problems[p] = ProblemState();
    }

    void computeRankingStats() {
        solved_count = 0;
        total_penalty = 0;
        sorted_solve_times.clear();
        for (auto &[p, state] : problems) {
            if (state.countsForRanking() && state.solved) {
                solved_count++;
                int penalty = 20 * state.wrong_attempts_before + state.solve_time;
                total_penalty += penalty;
                sorted_solve_times.push_back(state.solve_time);
            }
        }
        sort(sorted_solve_times.rbegin(), sorted_solve_times.rend());
    }

    bool hasFrozenProblems() const {
        for (const auto &[p, state] : problems) {
            if (state.frozen) return true;
        }
        return false;
    }

    char getSmallestFrozenProblem() const {
        char min_p = 'Z' + 1;
        for (const auto &[p, state] : problems) {
            if (state.frozen && p < min_p) {
                min_p = p;
            }
        }
        return min_p;
    }
};

bool compareTeams(const Team *a, const Team *b) {
    if (a->solved_count != b->solved_count) {
        return a->solved_count > b->solved_count;
    }
    if (a->total_penalty != b->total_penalty) {
        return a->total_penalty < b->total_penalty;
    }
    const vector<int> &sa = a->sorted_solve_times;
    const vector<int> &sb = b->sorted_solve_times;
    for (size_t i = 0; i < sa.size() && i < sb.size(); i++) {
        if (sa[i] != sb[i]) {
            return sa[i] < sb[i];
        }
    }
    return a->name < b->name;
}

class ICPCSystem {
private:
    bool competition_started = false;
    bool is_frozen = false;
    int duration_time;
    int problem_count;
    unordered_map<string, Team*> teams_map;
    list<Team*> current_ranking;

public:
    ICPCSystem() : duration_time(0), problem_count(0) {}

    ~ICPCSystem() {
        for (auto team : teams_map) {
            delete team.second;
        }
    }

    bool addTeam(const string &name, string &output) {
        if (competition_started) {
            output = "[Error]Add failed: competition has started.\n";
            return false;
        }
        if (teams_map.count(name)) {
            output = "[Error]Add failed: duplicated team name.\n";
            return false;
        }
        Team *team = new Team(name);
        teams_map[name] = team;
        current_ranking.push_back(team);
        sortCurrentRanking();
        output = "[Info]Add successfully.\n";
        return true;
    }

    bool startCompetition(int duration, int problems, string &output) {
        if (competition_started) {
            output = "[Error]Start failed: competition has started.\n";
            return false;
        }
        competition_started = true;
        duration_time = duration;
        problem_count = problems;
        for (int i = 0; i < problem_count; i++) {
            char p = 'A' + i;
            for (auto &[name, team] : teams_map) {
                team->addProblem(p);
            }
        }
        sortCurrentRanking();
        output = "[Info]Competition starts.\n";
        return true;
    }

    void submit(char problem, const string &team_name, Status status, int time) {
        Team *team = teams_map[team_name];
        Submission sub(problem, status, time);
        team->submissions.push_back(sub);

        ProblemState &ps = team->problems[problem];

        if (is_frozen) {
            if (!ps.solved_before_freeze) {
                ps.submissions_after++;
                ps.frozen = true;
            }
        }

        if (status == Status::Accepted) {
            if (!ps.solved) {
                ps.solved = true;
                ps.solve_time = time;
                if (!is_frozen || ps.solved_before_freeze) {
                    if (!ps.solved_before_freeze) {
                        ps.solved_before_freeze = true;
                    }
                }
            }
        } else {
            if (!ps.solved) {
                if (!is_frozen || ps.solved_before_freeze) {
                    ps.wrong_attempts_before++;
                }
            }
        }

        if (!is_frozen && !ps.solved_before_freeze && ps.solved) {
            ps.solved_before_freeze = true;
        }
    }

    void flush(string &output) {
        sortCurrentRanking();
        output = "[Info]Flush scoreboard.\n";
    }

    bool freeze(string &output) {
        if (is_frozen) {
            output = "[Error]Freeze failed: scoreboard has been frozen.\n";
            return false;
        }
        is_frozen = true;
        for (auto &[name, team] : teams_map) {
            for (auto &[p, state] : team->problems) {
                if (state.solved && state.countsForRanking()) {
                    state.solved_before_freeze = true;
                }
                if (!state.solved_before_freeze) {
                    state.submissions_after = 0;
                }
            }
        }
        output = "[Info]Freeze scoreboard.\n";
        return true;
    }

    bool scroll(string &output) {
        if (!is_frozen) {
            output = "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return false;
        }
        output = "[Info]Scroll scoreboard.\n";

        // First flush to get ranking before scrolling - output this
        recomputeRanking();
        output += scoreboardToString();

        // We need to repeatedly:
        // 1. Find lowest-ranked team that has frozen problems (search from bottom up)
        // 2. Unfreeze its smallest frozen problem
        // 3. Update ranking
        // 4. Output any ranking change that happened
        // Repeat until no frozen problems left.
        // Using linked list to make insertion/deletion more efficient

        while (true) {
            // Search from the end backwards for the first team with frozen problems
            list<Team*>::reverse_iterator it;
            bool found = false;
            Team *lowest_team = nullptr;
            for (it = current_ranking.rbegin(); it != current_ranking.rend(); ++it) {
                Team *t = *it;
                if (t->hasFrozenProblems()) {
                    lowest_team = t;
                    found = true;
                    break;
                }
            }
            if (!found) break;

            char p = lowest_team->getSmallestFrozenProblem();
            ProblemState &ps = lowest_team->problems[p];

            // We need to remove and reinsert because the sorted order changed
            // Erase from current position (list is ordered)
            auto node_it = find_it(lowest_team);
            current_ranking.erase(node_it);

            ps.frozen = false;
            if (ps.solved && !ps.solved_before_freeze) {
                ps.wrong_attempts_before += ps.submissions_after;
                ps.solved_before_freeze = true;
            }

            lowest_team->computeRankingStats();

            // Find new position in sorted list and insert
            auto insert_pos = find_insert_position(lowest_team);
            Team *overtaken = nullptr;
            int old_pos = 0; // unused for list
            bool output_change = (insert_pos != current_ranking.end());
            if (output_change) {
                overtaken = *insert_pos;
            }

            current_ranking.insert(insert_pos, lowest_team);

            // Output if ranking changed (team moved up)
            if (output_change) {
                output += lowest_team->name + " " + overtaken->name + " " + to_string(lowest_team->solved_count) + " " + to_string(lowest_team->total_penalty) + "\n";
            }
        }

        is_frozen = false;
        output += scoreboardToString();
        return true;
    }

    bool queryRanking(const string &team_name, string &output) {
        if (!teams_map.count(team_name)) {
            output = "[Error]Query ranking failed: cannot find the team.\n";
            return false;
        }
        output = "[Info]Complete query ranking.\n";
        if (is_frozen) {
            output += "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }
        int rank = 0;
        for (auto t : current_ranking) {
            rank++;
            if (t->name == team_name) {
                break;
            }
        }
        output += "[" + team_name + "] NOW AT RANKING [" + to_string(rank) + "]\n";
        return true;
    }

    bool querySubmission(const string &team_name, const string &problem_filter, const string &status_filter, string &output) {
        if (!teams_map.count(team_name)) {
            output = "[Error]Query submission failed: cannot find the team.\n";
            return false;
        }
        output = "[Info]Complete query submission.\n";
        Team *team = teams_map[team_name];
        const vector<Submission> &subs = team->submissions;
        for (auto it = subs.rbegin(); it != subs.rend(); ++it) {
            bool match_p = (problem_filter == "ALL") || (problem_filter.size() == 1 && problem_filter[0] == it->problem);
            bool match_s = (status_filter == "ALL") || (statusToString(it->status) == status_filter);
            if (match_p && match_s) {
                output += team_name + " " + string(1, it->problem) + " " + statusToString(it->status) + " " + to_string(it->time) + "\n";
                return true;
            }
        }
        output += "Cannot find any submission.\n";
        return true;
    }

    void endCompetition(string &output) {
        output = "[Info]Competition ends.\n";
    }

    void sortCurrentRanking() {
        current_ranking.sort(compareTeams);
    }

    void recomputeRanking() {
        for (auto team : current_ranking) {
            team->computeRankingStats();
        }
        current_ranking.sort(compareTeams);
    }

    list<Team*>::iterator find_it(Team *team) {
        for (auto it = current_ranking.begin(); it != current_ranking.end(); ++it) {
            if (*it == team) {
                return it;
            }
        }
        return current_ranking.end();
    }

    list<Team*>::iterator find_insert_position(Team *team) {
        for (auto it = current_ranking.begin(); it != current_ranking.end(); ++it) {
            if (!compareTeams(team, *it)) {
                return it;
            }
        }
        return current_ranking.end();
    }

    string scoreboardToString() const {
        string result;
        int rank = 1;
        for (const Team *team : current_ranking) {
            result += team->name + " " + to_string(rank) + " " + to_string(team->solved_count) + " " + to_string(team->total_penalty);
            for (int i = 0; i < problem_count; i++) {
                char p = 'A' + i;
                const ProblemState &ps = team->problems.at(p);
                result += " ";
                if (!ps.frozen) {
                    if (ps.countsForRanking() && ps.solved) {
                        if (ps.wrong_attempts_before == 0) {
                            result += "+";
                        } else {
                            result += "+" + to_string(ps.wrong_attempts_before);
                        }
                    } else {
                        int wrong = ps.wrong_attempts_before;
                        if (wrong == 0) {
                            result += ".";
                        } else {
                            result += "-" + to_string(wrong);
                        }
                    }
                } else {
                    int x = ps.wrong_attempts_before;
                    int y = ps.submissions_after;
                    if (x == 0) {
                        result += "0/" + to_string(y);
                    } else {
                        result += "-" + to_string(x) + "/" + to_string(y);
                    }
                }
            }
            result += "\n";
            rank++;
        }
        return result;
    }

    bool isCompetitionStarted() const { return competition_started; }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string cmd;

    while (cin >> cmd) {
        if (cmd == "ADDTEAM") {
            string team_name;
            cin >> team_name;
            string output;
            system.addTeam(team_name, output);
            cout << output;
        } else if (cmd == "START") {
            string duration_str, problem_str;
            int duration, problems;
            cin >> duration_str >> duration >> problem_str >> problems;
            string output;
            system.startCompetition(duration, problems, output);
            cout << output;
        } else if (cmd == "SUBMIT") {
            string problem, by, team_name, with, status_str, at;
            int time;
            cin >> problem >> by >> team_name >> with >> status_str >> at >> time;
            Status s = parseStatus(status_str);
            system.submit(problem[0], team_name, s, time);
        } else if (cmd == "FLUSH") {
            string output;
            system.flush(output);
            cout << output;
        } else if (cmd == "FREEZE") {
            string output;
            system.freeze(output);
            cout << output;
        } else if (cmd == "SCROLL") {
            string output;
            system.scroll(output);
            cout << output;
        } else if (cmd == "QUERY_RANKING") {
            string team_name;
            cin >> team_name;
            string output;
            system.queryRanking(team_name, output);
            cout << output;
        } else if (cmd == "QUERY_SUBMISSION") {
            string team_name;
            cin >> team_name;
            // Read the rest of the line to parse correctly
            string line;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            getline(cin, line);
            string problem_filter, status_filter;
            size_t problem_pos = line.find("PROBLEM=");
            size_t status_pos = line.find("STATUS=");
            if (problem_pos != string::npos) {
                problem_pos += 8;
                size_t end_problem = line.find(' ', problem_pos);
                if (end_problem == string::npos) {
                    problem_filter = line.substr(problem_pos);
                } else {
                    problem_filter = line.substr(problem_pos, end_problem - problem_pos);
                }
            }
            if (status_pos != string::npos) {
                status_pos += 7;
                status_filter = line.substr(status_pos);
                size_t end_status = status_filter.find(' ');
                if (end_status != string::npos) {
                    status_filter = status_filter.substr(0, end_status);
                }
            }
            string output;
            system.querySubmission(team_name, problem_filter, status_filter, output);
            cout << output;
        } else if (cmd == "END") {
            string output;
            system.endCompetition(output);
            cout << output;
            break;
        }
    }

    return 0;
}
