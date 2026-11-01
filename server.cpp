// ============================================================
// server.cpp - Hospital Management System backend
// Data handling + REST API (patients, doctors, appointments)
// Uses cpp-httplib (httplib.h, downloaded automatically by run.bat)
// ============================================================

#include "httplib.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// ------------------------------------------------------------
// Data structures
// ------------------------------------------------------------
struct Patient {
    int id;
    string name, age, gender, disease;
};

struct Doctor {
    int id;
    string name, speciality;
};

struct Appointment {
    int id;
    int patientId, doctorId;
    string date;
};

// ------------------------------------------------------------
// Global data
// ------------------------------------------------------------
vector<Patient> patients;
vector<Doctor> doctors;
vector<Appointment> appointments;

int nextId = 1;
mutex dataLock;
const string DB_FILE = "data.txt";

// ------------------------------------------------------------
// Helper functions
// ------------------------------------------------------------

// Escape text for JSON
string jsonEscape(const string &text) {
    string out;
    for (char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

// Remove characters that would break our data file
string clean(string text) {
    for (char &c : text) {
        if (c == '|' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return text;
}

// Split a line by '|'
vector<string> split(const string &line) {
    vector<string> parts;
    string piece;
    stringstream stream(line);
    while (getline(stream, piece, '|')) {
        parts.push_back(piece);
    }
    return parts;
}

// Send a JSON reply
void sendJson(httplib::Response &res, const string &body, int status = 200) {
    res.status = status;
    res.set_content(body, "application/json");
}

// Delete an item by id from any vector
template <class T> bool removeById(vector<T> &list, int id) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].id == id) {
            list.erase(list.begin() + i);
            return true;
        }
    }
    return false;
}

string patientName(int id) {
    for (auto &p : patients) {
        if (p.id == id) {
            return p.name;
        }
    }
    return "(deleted)";
}

string doctorName(int id) {
    for (auto &d : doctors) {
        if (d.id == id) {
            return d.name;
        }
    }
    return "(deleted)";
}

// ------------------------------------------------------------
// Save / load data file
// ------------------------------------------------------------
void saveData() {
    ofstream file(DB_FILE);

    for (auto &p : patients) {
        file << "P|" << p.id << '|' << p.name << '|' << p.age << '|' << p.gender << '|'
             << p.disease << '\n';
    }
    for (auto &d : doctors) {
        file << "D|" << d.id << '|' << d.name << '|' << d.speciality << '\n';
    }
    for (auto &a : appointments) {
        file << "A|" << a.id << '|' << a.patientId << '|' << a.doctorId << '|' << a.date
             << '\n';
    }
}

void loadData() {
    ifstream file(DB_FILE);
    string line;

    while (getline(file, line)) {
        vector<string> v = split(line);
        if (v.size() < 3) {
            continue;
        }

        int id = stoi(v[1]);
        nextId = max(nextId, id + 1);

        if (v[0] == "P" && v.size() >= 6) {
            patients.push_back({id, v[2], v[3], v[4], v[5]});
        } else if (v[0] == "D" && v.size() >= 4) {
            doctors.push_back({id, v[2], v[3]});
        } else if (v[0] == "A" && v.size() >= 5) {
            appointments.push_back({id, stoi(v[2]), stoi(v[3]), v[4]});
        }
    }
}

// ------------------------------------------------------------
// Patient routes
// ------------------------------------------------------------
void patientRoutes(httplib::Server &server) {

    // List patients (optional ?q=search)
    server.Get("/api/patients", [](const httplib::Request &req, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        string query = req.has_param("q") ? req.get_param_value("q") : "";
        string out = "[";

        for (auto &p : patients) {
            bool matches = query.empty() || p.name.find(query) != string::npos ||
                           p.disease.find(query) != string::npos;
            if (!matches) {
                continue;
            }
            if (out.size() > 1) {
                out += ",";
            }
            out += "{\"id\":" + to_string(p.id) + ",\"name\":\"" + jsonEscape(p.name) +
                   "\",\"age\":\"" + jsonEscape(p.age) + "\",\"gender\":\"" +
                   jsonEscape(p.gender) + "\",\"disease\":\"" + jsonEscape(p.disease) + "\"}";
        }

        sendJson(res, out + "]");
    });

    // Add patient
    server.Post("/api/patients", [](const httplib::Request &req, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        string name = clean(req.get_param_value("name"));
        if (name.empty()) {
            sendJson(res, "{\"error\":\"Name is required\"}", 400);
            return;
        }

        patients.push_back({nextId++, name, clean(req.get_param_value("age")),
                            clean(req.get_param_value("gender")),
                            clean(req.get_param_value("disease"))});
        saveData();
        sendJson(res, "{\"ok\":true}", 201);
    });

    // Delete patient
    server.Delete(R"(/api/patients/(\d+))", [](const httplib::Request &req,
                                                httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        bool found = removeById(patients, stoi(req.matches[1]));
        saveData();
        if (found) {
            sendJson(res, "{\"ok\":true}");
        } else {
            sendJson(res, "{\"error\":\"Not found\"}", 404);
        }
    });
}

// ------------------------------------------------------------
// Doctor routes
// ------------------------------------------------------------
void doctorRoutes(httplib::Server &server) {

    // List doctors
    server.Get("/api/doctors", [](const httplib::Request &, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        string out = "[";
        for (auto &d : doctors) {
            if (out.size() > 1) {
                out += ",";
            }
            out += "{\"id\":" + to_string(d.id) + ",\"name\":\"" + jsonEscape(d.name) +
                   "\",\"speciality\":\"" + jsonEscape(d.speciality) + "\"}";
        }

        sendJson(res, out + "]");
    });

    // Add doctor
    server.Post("/api/doctors", [](const httplib::Request &req, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        string name = clean(req.get_param_value("name"));
        if (name.empty()) {
            sendJson(res, "{\"error\":\"Name is required\"}", 400);
            return;
        }

        doctors.push_back({nextId++, name, clean(req.get_param_value("speciality"))});
        saveData();
        sendJson(res, "{\"ok\":true}", 201);
    });

    // Delete doctor
    server.Delete(R"(/api/doctors/(\d+))", [](const httplib::Request &req,
                                               httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        bool found = removeById(doctors, stoi(req.matches[1]));
        saveData();
        if (found) {
            sendJson(res, "{\"ok\":true}");
        } else {
            sendJson(res, "{\"error\":\"Not found\"}", 404);
        }
    });
}

// ------------------------------------------------------------
// Appointment routes
// ------------------------------------------------------------
void appointmentRoutes(httplib::Server &server) {

    // List appointments
    server.Get("/api/appointments", [](const httplib::Request &, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        string out = "[";
        for (auto &a : appointments) {
            if (out.size() > 1) {
                out += ",";
            }
            out += "{\"id\":" + to_string(a.id) + ",\"patient\":\"" +
                   jsonEscape(patientName(a.patientId)) + "\",\"doctor\":\"" +
                   jsonEscape(doctorName(a.doctorId)) + "\",\"date\":\"" + jsonEscape(a.date) +
                   "\"}";
        }

        sendJson(res, out + "]");
    });

    // Book appointment
    server.Post("/api/appointments", [](const httplib::Request &req, httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        try {
            int patientId = stoi(req.get_param_value("patientId"));
            int doctorId = stoi(req.get_param_value("doctorId"));
            string date = clean(req.get_param_value("date"));

            if (date.empty()) {
                sendJson(res, "{\"error\":\"Pick a date and time\"}", 400);
                return;
            }

            // Same doctor cannot be booked twice at the same time
            for (auto &a : appointments) {
                if (a.doctorId == doctorId && a.date == date) {
                    sendJson(res, "{\"error\":\"This doctor is already booked at that time\"}",
                             409);
                    return;
                }
            }

            appointments.push_back({nextId++, patientId, doctorId, date});
            saveData();
            sendJson(res, "{\"ok\":true}", 201);
        } catch (...) {
            sendJson(res, "{\"error\":\"Choose a patient and a doctor\"}", 400);
        }
    });

    // Cancel appointment
    server.Delete(R"(/api/appointments/(\d+))", [](const httplib::Request &req,
                                                    httplib::Response &res) {
        lock_guard<mutex> guard(dataLock);

        bool found = removeById(appointments, stoi(req.matches[1]));
        saveData();
        if (found) {
            sendJson(res, "{\"ok\":true}");
        } else {
            sendJson(res, "{\"error\":\"Not found\"}", 404);
        }
    });
}

// ------------------------------------------------------------
// Send a frontend file (html / css / js) to the browser
// ------------------------------------------------------------
void sendFile(httplib::Response &res, const string &path, const string &type) {
    ifstream file(path, ios::binary);
    if (!file) {
        res.status = 404;
        res.set_content("File not found", "text/plain");
        return;
    }
    stringstream content;
    content << file.rdbuf();
    res.set_content(content.str(), type);
}

// ------------------------------------------------------------
// Start the server (called from main.cpp)
// ------------------------------------------------------------
void startServer() {
    loadData();

    httplib::Server server;

    // Frontend pages
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        sendFile(res, "index.html", "text/html; charset=utf-8");
    });
    server.Get("/style.css", [](const httplib::Request &, httplib::Response &res) {
        sendFile(res, "style.css", "text/css");
    });
    server.Get("/script.js", [](const httplib::Request &, httplib::Response &res) {
        sendFile(res, "script.js", "text/javascript");
    });

    // API
    patientRoutes(server);
    doctorRoutes(server);
    appointmentRoutes(server);

    cout << "Server running at http://localhost:8080\n";
    cout << "Press Ctrl+C to stop.\n";
    server.listen("0.0.0.0", 8080);
}
