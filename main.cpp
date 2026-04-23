#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <memory>
#include <cassert>

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

    // Does this problem count for ranking in current state?
    bool countsForRanking() const {
        if (solved_before_freeze) return true;
        if (frozen) return false;
        return solved;
    }

    int getWrongAttempts() const {
        if (solved_before_freeze || !frozen) {
            return wrong_attempts_before;
        } else {
            return wrong_attempts_before;
        }
    }
};

class Team {
public:
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;
    int solved_count = 0;
    int total_penalty = 0;

    // For ranking comparison - store sorted solve times
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
    const auto &sa = a->sorted_solve_times;
    const auto &sb = b->sorted_solve_times;
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
    vector<Team*> all_teams;
    vector<Team*> current_ranking;
    vector<Submission> all_submissions;
    bool ranking_dirty = true;

public:
    ICPCSystem() : duration_time(0), problem_count(0) {}

    ~ICPCSystem() {
        for (auto team : all_teams) {
            delete team;
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
        all_teams.push_back(team);
        ranking_dirty = true;
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
            for (auto team : all_teams) {
                team->addProblem(p);
            }
        }
        ranking_dirty = true;
        output = "[Info]Competition starts.\n";
        return true;
    }

    void submit(char problem, const string &team_name, Status status, int time) {
        Team *team = teams_map[team_name];
        Submission sub(problem, status, time);
        team->submissions.push_back(sub);
        all_submissions.push_back(sub);

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
                    // counts immediately
                } else {
                    // frozen - doesn't count yet
                }
                ranking_dirty = true;
            }
        } else {
            if (!ps.solved) {
                if (!is_frozen || ps.solved_before_freeze) {
                    ps.wrong_attempts_before++;
                }
                // else wrong attempts after freeze don't count toward before
                ranking_dirty = true;
            }
        }

        if (!is_frozen && !ps.solved_before_freeze && ps.solved) {
            ps.solved_before_freeze = true;
        }
    }

    void flush(string &output) {
        recomputeRanking();
        output = "[Info]Flush scoreboard.\n";
    }

    bool freeze(string &output) {
        if (is_frozen) {
            output = "[Error]Freeze failed: scoreboard has been frozen.\n";
            return false;
        }
        is_frozen = true;
        for (auto team : all_teams) {
            for (auto &[p, state] : team->problems) {
                if (state.solved && state.countsForRanking()) {
                    state.solved_before_freeze = true;
                }
                if (!state.solved_before_freeze && state.submissions_after > 0) {
                    state.frozen = true;
                }
            }
        }
        output = "[Info]Freeze scoreboard.\n";
        ranking_dirty = true;
        return true;
    }

    bool scroll(string &output) {
        if (!is_frozen) {
            output = "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return false;
        }
        output = "[Info]Scroll scoreboard.\n";

        // First flush to get ranking before scrolling
        recomputeRanking();
        output += scoreboardToString();

        vector<pair<Team*, char>> unfreeze_order;

        // Collect all frozen problems in order of scrolling
        while (true) {
            bool found = false;
            Team *lowest_team = nullptr;
            for (auto it = current_ranking.rbegin(); it != current_ranking.rend(); ++it) {
                Team *t = *it;
                if (t->hasFrozenProblems()) {
                    lowest_team = t;
                    found = true;
                    break;
                }
            }
            if (!found) break;

            char p = lowest_team->getSmallestFrozenProblem();
            unfreeze_order.emplace_back(lowest_team, p);
            ProblemState &ps = lowest_team->problems[p];
            ps.frozen = false;
            if (ps.solved) {
                if (!ps.solved_before_freeze) {
                    ps.wrong_attempts_before += ps.submissions_after;
                    ps.solved_before_freeze = true;
                }
            }
            recomputeRanking();
        }

        // Now output ranking changes
        recomputeRanking();
        for (auto &[team, p] : unfreeze_order) {
            // We need to output whenever unfreezing caused a ranking change
            // For this problem, we just need to output each swap that happens
            // But actually the spec says: output each unfreeze that causes ranking change
            // We'll handle this by recomputing before and after each step and checking
            // To keep it simple, let's redo the unfreezing step by step and detect changes
        }

        // To correctly output ranking changes, we need to do this incrementally:
        // Start from pre-scroll ranking, unfreeze one by one and output changes

        // First, restore freeze state and recompute initial ranking
        for (auto &[team, p] : unfreeze_order) {
            team->problems[p].frozen = true;
        }
        recomputeRanking();
        vector<int> old_pos(all_teams.size());
        for (int i = 0; i < (int)current_ranking.size(); i++) {
            auto it = find_if(all_teams.begin(), all_teams.end(), [&](Team *t) { return t == current_ranking[i]; });
            int idx = it - all_teams.begin();
            old_pos[idx] = i;
        }

        for (auto &[team, p] : unfreeze_order) {
            team->problems[p].frozen = false;
            recomputeRanking();

            vector<int> new_pos(all_teams.size());
            for (int i = 0; i < (int)current_ranking.size(); i++) {
                auto it = find_if(all_teams.begin(), all_teams.end(), [&](Team *t) { return t == current_ranking[i]; });
                int idx = it - all_teams.begin();
                new_pos[idx] = i;
            }

            auto it_team = find_if(all_teams.begin(), all_teams.end(), [&](Team *t) { return t == team; });
            int idx_team = it_team - all_teams.begin();
            if (new_pos[idx_team] != old_pos[idx_team]) {
                // Ranking changed - output the change
                // Find the team that was overtaken
                int new_rank = new_pos[idx_team] + 1;
                int old_rank = old_pos[idx_team] + 1;
                if (new_rank < old_rank) {
                    Team *overtaken = current_ranking[new_rank - 1 + 1 - 1];
                    output += team->name + " " + overtaken->name + " " + to_string(team->solved_count) + " " + to_string(team->total_penalty) + "\n";
                }
            }

            old_pos = new_pos;
        }

        recomputeRanking();
        output += scoreboardToString();
        is_frozen = false;
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
        recomputeRankingIfNeeded();
        int rank = 0;
        for (int i = 0; i < (int)current_ranking.size(); i++) {
            if (current_ranking[i]->name == team_name) {
                rank = i + 1;
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

    void recomputeRanking() {
        for (auto team : all_teams) {
            team->computeRankingStats();
        }
        current_ranking.clear();
        for (auto team : all_teams) {
            current_ranking.push_back(team);
        }
        if (current_ranking.empty()) {
            ranking_dirty = false;
            return;
        }
        // Before any flush, sort by name
        if (ranking_dirty || !competition_started) {
            sort(current_ranking.begin(), current_ranking.end(), [](Team *a, Team *b) {
                return a->name < b->name;
            });
        } else {
            sort(current_ranking.begin(), current_ranking.end(), compareTeams);
        }
        if (competition_started) {
            sort(current_ranking.begin(), current_ranking.end(), compareTeams);
        }
        ranking_dirty = false;
    }

    void recomputeRankingIfNeeded() {
        if (ranking_dirty) {
            recomputeRanking();
        }
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
            string team_name, where, problem_part, problem_val, and_token, status_val;
            cin >> team_name >> where >> problem_part >> problem_val >> and_token >> status_val;
            // problem_val is like "PROBLEM=A" or "PROBLEM=ALL" - the last token is actually the value
            // when parsing with cin, we got problem_val as "A" or "ALL" because it splits on =
            // Wait, actually input is "QUERY_SUBMISSION Team_Rocket WHERE PROBLEM=ALL AND STATUS=ALL"
            // With cin whitespace split, this could be:
            // team_name = Team_Rocket
            // where = WHERE
            // problem_part = PROBLEM=ALL
            // and_token = AND
            // status_val = STATUS=ALL
            // OR if the = is split:
            // team_name = Team_Rocket
            // where = WHERE
            // problem_part = PROBLEM=
            // problem_val = ALL
            // and_token = AND
            // status_val = STATUS=
            // status_val2 = ALL
            // Let me handle both cases
            string problem_filter, status_filter;
            size_t eq;
            if (problem_part.find('=') != string::npos) {
                eq = problem_part.find('=');
                problem_filter = problem_part.substr(eq + 1);
                if (problem_filter.empty()) {
                    problem_filter = problem_val;
                }
            } else {
                problem_filter = problem_val;
            }
            if (status_val.find('=') != string::npos) {
                eq = status_val.find('=');
                status_filter = status_val.substr(eq + 1);
            } else {
                // should not happen with input format
                status_filter = status_val;
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
