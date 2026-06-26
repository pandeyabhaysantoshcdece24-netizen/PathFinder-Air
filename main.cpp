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

const double INF = std::numeric_limits<double>::infinity();
const std::string SAVE_FILE = "bookings.csv";

enum class TicketStatus { Confirmed, Cancelled };

struct Flight {
    std::string flightNumber;
    std::string destination;
    double price;
    double duration; 
    bool isCancelled = false;
    int availableSeats;

    Flight(std::string num, std::string dest, double p, double dur, int seats = 180)
        : flightNumber(num), destination(dest), price(p), duration(dur), availableSeats(seats) {}
};

struct Ticket {
    std::string ticketID;
    std::string passengerName;
    std::string passportNumber;
    std::string flightNumber;
    std::string source;
    std::string destination;
    double farePaid;
    TicketStatus status;
};

class AirlineEngine {
private:
    std::unordered_map<std::string, std::vector<Flight>> routes;
    std::vector<std::string> airports;
    std::unordered_map<std::string, Ticket> tickets; 

    std::string generatePNR() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(100000, 999999);
        return "PNR" + std::to_string(distrib(gen));
    }

public:
    void addAirport(const std::string& code) {
        if (routes.find(code) == routes.end()) {
            routes[code] = std::vector<Flight>();
            airports.push_back(code);
        }
    }

    void addFlight(const std::string& src, const std::string& dest, const std::string& flightNum, double price, double duration, int seats = 180) {
        addAirport(src);
        addAirport(dest);
        routes[src].push_back(Flight(flightNum, dest, price, duration, seats));
    }

    bool showDirectFlights(const std::string& src, const std::string& dest) {
        if (routes.find(src) == routes.end()) return false;
        
        bool found = false;
        for (const auto& flight : routes.at(src)) {
            if (flight.destination == dest && !flight.isCancelled) {
                if (!found) {
                    std::cout << "\n--- Available Direct Flights Available ---\n";
                    found = true;
                }
                std::cout << " -> Flight No: " << flight.flightNumber 
                          << " | Price: ₹" << flight.price 
                          << " | Duration: " << flight.duration << " hrs"
                          << " | Seats Left: " << flight.availableSeats << "\n";
            }
        }
        return found;
    }

    // ========================================================================
    // FILE HANDLER METHODS (PERSISTENCE LAYER)
    // ========================================================================
    void saveBookingsToFile() {
        std::ofstream file(SAVE_FILE, std::ios::trunc); // Overwrite file fresh
        if (!file.is_open()) {
            std::cout << "[SYSTEM ERROR] Could not open database file to write data.\n";
            return;
        }

        for (const auto& [pnr, ticket] : tickets) {
            file << ticket.ticketID << ","
                 << ticket.passengerName << ","
                 << ticket.passportNumber << ","
                 << ticket.flightNumber << ","
                 << ticket.source << ","
                 << ticket.destination << ","
                 << ticket.farePaid << ","
                 << static_cast<int>(ticket.status) << "\n";
        }
        file.close();
    }

    void loadBookingsFromFile() {
        std::ifstream file(SAVE_FILE);
        if (!file.is_open()) {
            // File doesn't exist yet, which is completely fine on the first run
            return; 
        }

        std::string line;
        int activeCount = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            std::string pnr, name, passport, flightNum, src, dest, fareStr, statusStr;

            std::getline(ss, pnr, ',');
            std::getline(ss, name, ',');
            std::getline(ss, passport, ',');
            std::getline(ss, flightNum, ',');
            std::getline(ss, src, ',');
            std::getline(ss, dest, ',');
            std::getline(ss, fareStr, ',');
            std::getline(ss, statusStr, ',');

            double fare = std::stod(fareStr);
            TicketStatus status = (statusStr == "1") ? TicketStatus::Cancelled : TicketStatus::Confirmed;

            // Rebuild ticket record
            Ticket loadedTicket{pnr, name, passport, flightNum, src, dest, fare, status};
            tickets[pnr] = loadedTicket;

            // Deduct seat from flight inventory if the ticket is active/confirmed
            if (status == TicketStatus::Confirmed && routes.find(src) != routes.end()) {
                for (auto& flight : routes[src]) {
                    if (flight.flightNumber == flightNum && flight.destination == dest) {
                        flight.availableSeats--;
                        break;
                    }
                }
            }
            activeCount++;
        }
        file.close();
        if (activeCount > 0) {
            std::cout << "[SYSTEM] Restored " << activeCount << " ledger items securely from " << SAVE_FILE << ".\n";
        }
    }

    std::string bookTicket(const std::string& name, const std::string& passport, 
                           const std::string& src, const std::string& dest, const std::string& flightNum, std::string explicitPNR = "") {
        if (routes.find(src) == routes.end()) return "";

        for (auto& flight : routes[src]) {
            if (flight.flightNumber == flightNum && flight.destination == dest) {
                if (flight.isCancelled || flight.availableSeats <= 0) return "";

                flight.availableSeats--;
                std::string pnr = explicitPNR.empty() ? generatePNR() : explicitPNR;
                tickets[pnr] = Ticket{pnr, name, passport, flightNum, src, dest, flight.price, TicketStatus::Confirmed};

                std::cout << "\n>>> [SUCCESS] Ticket Issued Successfully!\n";
                std::cout << "    PNR          : " << pnr << "\n";
                std::cout << "    Passenger    : " << name << "\n";
                std::cout << "    Flight No    : " << flightNum << " (" << src << " -> " << dest << ")\n";
                std::cout << "    Fare Charged : ₹" << flight.price << "\n";

                saveBookingsToFile(); // Auto-commit to file system
                return pnr;
            }
        }
        return "";
    }

    void cancelTicket(const std::string& pnr) {
        auto it = tickets.find(pnr);
        if (it == tickets.end()) {
            std::cout << "[ERROR] Invalid PNR sequence: " << pnr << "\n";
            return;
        }

        Ticket& ticket = it->second;
        if (ticket.status == TicketStatus::Cancelled) {
            std::cout << "[WARNING] Ticket " << pnr << " has already been processed as cancelled.\n";
            return;
        }

        for (auto& flight : routes[ticket.source]) {
            if (flight.flightNumber == ticket.flightNumber) {
                flight.availableSeats++;
                break;
            }
        }

        ticket.status = TicketStatus::Cancelled;
        std::cout << ">>> [SUCCESS] PNR " << pnr << " Cancelled. Refund of ₹" << ticket.farePaid << " processed.\n";
        
        saveBookingsToFile(); // Save the updated cancellation status to local disk file
    }

    void findCheapestRoute(const std::string& source, const std::string& target) {
        std::unordered_map<std::string, double> minPrice;
        std::unordered_map<std::string, std::string> parent;
        
        for (const auto& pair : routes) minPrice[pair.first] = INF;
        if (minPrice.find(source) == minPrice.end()) minPrice[source] = INF;
        if (minPrice.find(target) == minPrice.end()) minPrice[target] = INF;
        
        std::priority_queue<std::pair<double, std::string>, std::vector<std::pair<double, std::string>>, std::greater<>> pq;
        minPrice[source] = 0.0;
        pq.push({0.0, source});

        while (!pq.empty()) {
            auto [currentPrice, u] = pq.top();
            pq.pop();

            if (currentPrice > minPrice[u]) continue;
            if (u == target) break;

            for (const auto& flight : routes[u]) {
                if (flight.isCancelled) continue; 

                if (minPrice[u] + flight.price < minPrice[flight.destination]) {
                    minPrice[flight.destination] = minPrice[u] + flight.price;
                    parent[flight.destination] = u;
                    pq.push({minPrice[flight.destination], flight.destination});
                }
            }
        }

        if (minPrice[target] == INF) {
            std::cout << "[RECOMMENDATION INFO] No system flight paths found connecting " << source << " to " << target << ".\n";
            return;
        }

        std::vector<std::string> path;
        for (std::string curr = target; curr != ""; curr = parent[curr]) {
            path.push_back(curr);
            if (curr == source) break;
        }
        std::reverse(path.begin(), path.end());

        std::cout << " -> System Smart Recommendation (Cheapest Over-all Path): ₹" << minPrice[target] << " via Layout [ ";
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i] << (i == path.size() - 1 ? "" : " -> ");
        }
        std::cout << " ]\n";
    }

    void computeGlobalConnectivity() {
        size_t n = airports.size();
        std::unordered_map<std::string, size_t> airportIndex;
        for (size_t i = 0; i < n; ++i) airportIndex[airports[i]] = i;

        std::vector<std::vector<int>> hopMatrix(n, std::vector<int>(n, 1e7));
        for (size_t i = 0; i < n; ++i) hopMatrix[i][i] = 0;

        for (const auto& [src, flights] : routes) {
            for (const auto& f : flights) {
                if (!f.isCancelled) hopMatrix[airportIndex[src]][airportIndex[f.destination]] = 1; 
            }
        }

        for (size_t k = 0; k < n; ++k) {
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    if (hopMatrix[i][k] + hopMatrix[k][j] < hopMatrix[i][j]) {
                        hopMatrix[i][j] = hopMatrix[i][k] + hopMatrix[k][j];
                    }
                }
            }
        }

        std::cout << "\n--- INDIAN DOMESTIC CONNECTIVITY MATRIX (Min Flight Hops) ---\n      ";
        for (const auto& ap : airports) std::cout << ap << "   ";
        std::cout << "\n";
        for (size_t i = 0; i < n; ++i) {
            std::cout << airports[i] << "   ";
            for (size_t j = 0; j < n; ++j) {
                if (hopMatrix[i][j] > 1e5) std::cout << "∞   ";
                else std::cout << hopMatrix[i][j] << "   ";
            }
            std::cout << "\n";
        }
    }
};

int main() {
    AirlineEngine engine;

    // --- POPULATE GRAPH NETWORK ENVIRONMENT ---
    engine.addFlight("DEL", "BOM", "6E-2012", 5500.0, 2.10, 180); 
    engine.addFlight("DEL", "BOM", "AI-805",  6200.0, 2.15, 100); 
    engine.addFlight("DEL", "BLR", "AI-803",  7200.0, 2.45, 2);   
    engine.addFlight("DEL", "GOI", "UK-847",  8500.0, 2.30, 150); 
    engine.addFlight("DEL", "HYD", "6E-512",  4800.0, 2.15, 180); 
    engine.addFlight("DEL", "CCU", "AI-701",  6100.0, 2.10, 160); 

    engine.addFlight("BOM", "DEL", "6E-2342", 5400.0, 2.15, 180); 
    engine.addFlight("BOM", "BLR", "QP-1102", 4200.0, 1.45, 180); 
    engine.addFlight("BOM", "GOI", "6E-5314", 3800.0, 1.15, 180); 
    engine.addFlight("BOM", "HYD", "AI-615",  3900.0, 1.25, 150); 

    engine.addFlight("BLR", "DEL", "UK-812",  7400.0, 2.40, 150); 
    engine.addFlight("BLR", "GOI", "I5-1326", 3100.0, 1.20, 140); 
    engine.addFlight("BLR", "CCU", "6E-421",  6800.0, 2.35, 180); 

    engine.addFlight("HYD", "BLR", "6E-356",  3200.0, 1.05, 180); 
    engine.addFlight("HYD", "CCU", "QP-1512", 5900.0, 2.00, 180); 

    engine.addFlight("CCU", "DEL", "AI-702",  6300.0, 2.20, 160); 

    // --- RE-BOOT STORAGE RETRIEVAL HANDSHAKE ---
    engine.loadBookingsFromFile();

    int choice = 0;
    while (true) {
        std::cout << "\n==================================================\n";
        std::cout << "      REAL-TIME INDIAN DOMESTIC AIRLINE SYSTEM     \n";
        std::cout << "==================================================\n";
        std::cout << "1. Search Cheapest Point-to-Point Route (Dijkstra)\n";
        std::cout << "2. Live Passenger Ticket Booking\n";
        std::cout << "3. Cancel Ticket / Issue Refund via PNR\n";
        std::cout << "4. Generate Global Network Hops Matrix\n";
        std::cout << "5. Exit Operations Console\n";
        std::cout << "Enter system choice (1-5): ";
        std::cin >> choice;

        if (choice == 5) {
            std::cout << "\nSafely parking database clusters. Operations stopped. Goodbye!\n";
            break;
        }

        if (choice == 1) {
            std::string src, dest;
            std::cout << "\nEnter 3-Letter Source Code (e.g., DEL, BOM): ";
            std::cin >> src;
            std::cout << "Enter 3-Letter Destination Code (e.g., GOI, BLR): ";
            std::cin >> dest;
            
            std::transform(src.begin(), src.end(), src.begin(), ::toupper);
            std::transform(dest.begin(), dest.end(), dest.begin(), ::toupper);
            
            engine.findCheapestRoute(src, dest);
        } 
        else if (choice == 2) {
            std::string name, passport, src, dest, flightNum;
            std::cin.ignore(); 
            
            std::cout << "\nEnter Passenger Full Name: ";
            std::getline(std::cin, name);
            std::cout << "Enter Document/Passport ID: ";
            std::cin >> passport;
            std::cout << "Enter 3-Letter Source Airport: ";
            std::cin >> src;
            std::cout << "Enter 3-Letter Destination Airport: ";
            std::cin >> dest;
            
            std::transform(src.begin(), src.end(), src.begin(), ::toupper);
            std::transform(dest.begin(), dest.end(), dest.begin(), ::toupper);
            
            bool directFlightsExist = engine.showDirectFlights(src, dest);
            if (!directFlightsExist) {
                std::cout << "\n[NOTICE] No direct flights found from " << src << " to " << dest << ".\n";
            }
            
            std::cout << "--- Route Optimization Helper ---\n";
            engine.findCheapestRoute(src, dest);
            std::cout << "---------------------------------\n";

            std::cout << "\nBased on the options above, type the Flight Number you want to book: ";
            std::cin >> flightNum;
            std::transform(flightNum.begin(), flightNum.end(), flightNum.begin(), ::toupper);
            
            engine.bookTicket(name, passport, src, dest, flightNum);
        } 
        else if (choice == 3) {
            std::string pnr;
            std::cout << "\nEnter 9-Character PNR String to process: ";
            std::cin >> pnr;
            std::transform(pnr.begin(), pnr.end(), pnr.begin(), ::toupper);
            engine.cancelTicket(pnr);
        } 
        else if (choice == 4) {
            engine.computeGlobalConnectivity();
        } 
        else {
            std::cout << "[ALERT] Invalid execution instruction entry. Try again.\n";
        }
    }
    return 0;
}
