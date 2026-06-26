#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>

using namespace std;

const double INF = numeric_limits<double>::infinity();
const string DB_FILE = "bookings.csv";

enum class tstatus { ok, can };

struct flight {
    string fnum;
    string dest;
    double price;
    double dur; 
    bool dead = false;
    int seats;

    flight(string n, string d, double p, double dr, int s = 180)
        : fnum(n), dest(d), price(p), dur(dr), seats(s) {}
};

struct ticket {
    string tid;
    string name;
    string pass;
    string fnum;
    string src;
    string dest;
    double fare;
    tstatus status;
};

class engine {
private:
    unordered_map<string, vector<flight>> adj;
    vector<string> nodes;
    unordered_map<string, ticket> tx; 

    string gen_pnr() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(100000, 999999);
        return "pnr" + to_string(dist(gen));
    }

public:
    void add_node(const string& code) {
        if (adj.find(code) == adj.end()) {
            adj[code] = vector<flight>();
            nodes.push_back(code);
        }
    }

    void add_f(const string& src, const string& dest, const string& fnum, double p, double dr, int s = 180) {
        add_node(src);
        add_node(dest);
        adj[src].push_back(flight(fnum, dest, p, dr, s));
    }

    bool show_dir(const string& src, const string& dest) {
        if (adj.find(src) == adj.end()) return false;
        
        bool found = false;
        for (const auto& f : adj.at(src)) {
            if (f.dest == dest && !f.dead) {
                if (!found) {
                    cout << "\ndirect flights available:\n";
                    found = true;
                }
                cout << "flight: " << f.fnum 
                     << " | price: rs." << f.price 
                     << " | duration: " << f.dur << "h"
                     << " | seats: " << f.seats << "\n";
            }
        }
        return found;
    }

    void save_db() {
        ofstream file(DB_FILE, ios::trunc);
        if (!file.is_open()) {
            cout << "error: unable to save data.\n";
            return;
        }

        for (const auto& pair : tx) {
            ticket t = pair.second;
            file << t.tid << "," << t.name << "," << t.pass << ","
                 << t.fnum << "," << t.src << "," << t.dest << ","
                 << t.fare << "," << static_cast<int>(t.status) << "\n";
        }
        file.close();
    }

    void load_db() {
        ifstream file(DB_FILE);
        if (!file.is_open()) return; 

        string line;
        int count = 0;
        while (getline(file, line)) {
            if (line.empty()) continue;
            
            stringstream ss(line);
            string pnr, name, pass, fnum, src, dest, s_fare, s_stat;

            getline(ss, pnr, ','); getline(ss, name, ',');
            getline(ss, pass, ','); getline(ss, fnum, ',');
            getline(ss, src, ','); getline(ss, dest, ',');
            getline(ss, s_fare, ','); getline(ss, s_stat, ',');

            double fare = stod(s_fare);
            tstatus stat = (s_stat == "1") ? tstatus::can : tstatus::ok;

            tx[pnr] = ticket{pnr, name, pass, fnum, src, dest, fare, stat};

            if (stat == tstatus::ok && adj.find(src) != adj.end()) {
                for (auto& f : adj[src]) {
                    if (f.fnum == fnum && f.dest == dest) {
                        f.seats--;
                        break;
                    }
                }
            }
            count++;
        }
        file.close();
        if (count > 0) cout << "restored " << count << " bookings from database.\n";
    }

    string book(const string& name, const string& pass, const string& src, const string& dest, const string& fnum, string exp = "") {
        if (adj.find(src) == adj.end()) return "";

        for (auto& f : adj[src]) {
            if (f.fnum == fnum && f.dest == dest) {
                if (f.dead || f.seats <= 0) return "";

                f.seats--;
                string pnr = exp.empty() ? gen_pnr() : exp;
                tx[pnr] = ticket{pnr, name, pass, fnum, src, dest, f.price, tstatus::ok};

                cout << "\nticket booked successfully.\n"
                     << "pnr          : " << pnr << "\n"
                     << "passenger    : " << name << "\n"
                     << "flight       : " << fnum << " (" << src << " -> " << dest << ")\n"
                     << "price paid   : rs." << f.price << "\n";

                save_db(); 
                return pnr;
            }
        }
        return "";
    }

    void cancel(const string& pnr) {
        auto it = tx.find(pnr);
        if (it == tx.end()) {
            cout << "error: invalid pnr -> " << pnr << "\n";
            return;
        }

        ticket& t = it->second;
        if (t.status == tstatus::can) {
            cout << "ticket is already cancelled.\n";
            return;
        }

        for (auto& f : adj[t.src]) {
            if (f.fnum == t.fnum) {
                f.seats++;
                break;
            }
        }

        t.status = tstatus::can;
        cout << "cancellation processed. refund of rs." << t.fare << " issued.\n";
        save_db();
    }

    void find_path(const string& src, const string& dst) {
        unordered_map<string, double> dist;
        unordered_map<string, string> par;
        
        for (const auto& p : adj) dist[p.first] = INF;
        if (dist.find(src) == dist.end()) dist[src] = INF;
        if (dist.find(dst) == dist.end()) dist[dst] = INF;
        
        priority_queue<pair<double, string>, vector<pair<double, string>>, greater<pair<double, string>>> pq;
        dist[src] = 0.0;
        pq.push(make_pair(0.0, src));

        while (!pq.empty()) {
            auto top = pq.top();
            double d = top.first;
            string u = top.second;
            pq.pop();

            if (d > dist[u]) continue;
            if (u == dst) break;

            for (const auto& f : adj[u]) {
                if (f.dead) continue; 

                if (dist[u] + f.price < dist[f.dest]) {
                    dist[f.dest] = dist[u] + f.price;
                    par[f.dest] = u;
                    pq.push(make_pair(dist[f.dest], f.dest));
                }
            }
        }

        if (dist[dst] == INF) {
            cout << "no route found connecting " << src << " to " << dst << ".\n";
            return;
        }

        vector<string> path;
        for (string curr = dst; curr != ""; curr = par[curr]) {
            path.push_back(curr);
            if (curr == src) break;
        }
        reverse(path.begin(), path.end());

        cout << "cheapest path: rs." << dist[dst] << " via layout [ ";
        for (size_t i = 0; i < path.size(); ++i) {
            cout << path[i] << (i == path.size() - 1 ? "" : " -> ");
        }
        cout << " ]\n";
    }

    void get_net() {
        size_t n = nodes.size();
        unordered_map<string, size_t> idx;
        for (size_t i = 0; i < n; ++i) idx[nodes[i]] = i;

        vector<vector<int>> mat(n, vector<int>(n, 1e7));
        for (size_t i = 0; i < n; ++i) mat[i][i] = 0;

        for (const auto& p : adj) {
            string src = p.first;
            for (const auto& f : p.second) {
                if (!f.dead) mat[idx[src]][idx[f.dest]] = 1; 
            }
        }

        for (size_t k = 0; k < n; ++k) {
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    if (mat[i][k] + mat[k][j] < mat[i][j]) {
                        mat[i][j] = mat[i][k] + mat[k][j];
                    }
                }
            }
        }

        cout << "\nconnectivity matrix (min hops):\n      ";
        for (const auto& ap : nodes) cout << ap << "   ";
        cout << "\n";
        for (size_t i = 0; i < n; ++i) {
            cout << nodes[i] << "   ";
            for (size_t j = 0; j < n; ++j) {
                if (mat[i][j] > 1e5) cout << "-   ";
                else cout << mat[i][j] << "   ";
            }
            cout << "\n";
        }
    }
};

int main() {
    engine eng;

    eng.add_f("del", "bom", "6e-2012", 5500.0, 2.10); 
    eng.add_f("del", "bom", "ai-805",  6200.0, 2.15); 
    eng.add_f("del", "blr", "ai-803",  7200.0, 2.45, 2);   
    eng.add_f("del", "goi", "uk-847",  8500.0, 2.30); 
    eng.add_f("del", "hyd", "6e-512",  4800.0, 2.15); 
    eng.add_f("del", "ccu", "ai-701",  6100.0, 2.10); 

    eng.add_f("bom", "del", "6e-2342", 5400.0, 2.15); 
    eng.add_f("bom", "blr", "qp-1102", 4200.0, 1.45); 
    eng.add_f("bom", "goi", "6e-5314", 3800.0, 1.15); 
    eng.add_f("bom", "hyd", "ai-615",  3900.0, 1.25); 

    eng.add_f("blr", "del", "uk-812",  7400.0, 2.40); 
    eng.add_f("blr", "goi", "i5-1326", 3100.0, 1.20); 
    eng.add_f("blr", "ccu", "6e-421",  6800.0, 2.35); 

    eng.add_f("hyd", "blr", "6e-356",  3200.0, 1.05); 
    eng.add_f("hyd", "ccu", "qp-1512", 5900.0, 2.00); 

    eng.add_f("ccu", "del", "ai-702",  6300.0, 2.20); 

    eng.load_db();

    int choice = 0;
    while (true) {
        cout << "\nmenu options:\n"
             << "1. find route\n"
             << "2. book ticket\n"
             << "3. cancel ticket\n"
             << "4. network matrix\n"
             << "5. exit\n"
             << "choice: ";
        cin >> choice;

        if (choice == 5) break;

        if (choice == 1) {
            string src, dest;
            cout << "source: "; cin >> src;
            cout << "destination: "; cin >> dest;
            
            transform(src.begin(), src.end(), src.begin(), ::tolower);
            transform(dest.begin(), dest.end(), dest.begin(), ::tolower);
            
            eng.find_path(src, dest);
        } 
        else if (choice == 2) {
            string name, pass, src, dest, fnum;
            cin.ignore(); 
            
            cout << "passenger name: "; getline(cin, name);
            cout << "passport/id: "; cin >> pass;
            cout << "source: "; cin >> src;
            cout << "destination: "; cin >> dest;
            
            transform(src.begin(), src.end(), src.begin(), ::tolower);
            transform(dest.begin(), dest.end(), dest.begin(), ::tolower);
            
            if (!eng.show_dir(src, dest)) {
                cout << "no direct options found. checking connecting paths...\n";
            }
            eng.find_path(src, dest);

            cout << "enter flight number to confirm: ";
            cin >> fnum;
            transform(fnum.begin(), fnum.end(), fnum.begin(), ::tolower);
            
            eng.book(name, pass, src, dest, fnum);
        } 
        else if (choice == 3) {
            string pnr;
            cout << "enter pnr: ";
            cin >> pnr;
            transform(pnr.begin(), pnr.end(), pnr.begin(), ::tolower);
            eng.cancel(pnr);
        } 
        else if (choice == 4) {
            eng.get_net();
        } 
        else {
            cout << "invalid input.\n";
        }
    }
    return 0;
}c
