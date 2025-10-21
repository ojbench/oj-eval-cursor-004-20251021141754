#include <bits/stdc++.h>
using namespace std;

// Persistent storage helpers
namespace storage {
    static const string kDataDir = ".data";

    inline void ensureDataDir() {
        static bool ensured = false;
        if (ensured) return;
        std::error_code ec;
        std::filesystem::create_directories(kDataDir, ec);
        ensured = true;
    }

    inline string path(const string &name) {
        ensureDataDir();
        return kDataDir + string("/") + name;
    }
}

// Simple file-backed key-value store for accounts and books
struct Account {
    string userId;
    string password;
    int privilege = 1;
    string username;
};

struct Book {
    string isbn;
    string name;
    string author;
    string keyword;
    long long stock = 0;
    long double price = 0.0;
};

// Lightweight CSV-like escaping: fields cannot contain tabs or newlines by spec
static vector<string> splitTab(const string &s) {
    vector<string> v; string cur; 
    for (char c: s) {
        if (c == '\t') { v.push_back(cur); cur.clear(); }
        else if (c == '\r' || c == '\n') { /* ignore */ }
        else cur.push_back(c);
    }
    v.push_back(cur);
    return v;
}

static string joinTab(initializer_list<string> fields) {
    string out; bool first=true;
    for (auto &f: fields) {
        if (!first) out.push_back('\t');
        first=false; out += f;
    }
    return out;
}

// Account database
class AccountDB {
  public:
    AccountDB() { load(); }

    void load() {
        accounts.clear();
        ifstream fin(storage::path("accounts.tsv"));
        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            auto f = splitTab(line);
            if (f.size() < 4) continue;
            Account a{f[0], f[1], stoi(f[2]), f[3]};
            accounts[a.userId] = a;
        }
        // init root if missing
        if (!exists("root")) {
            Account root{"root", "sjtu", 7, "root"};
            accounts[root.userId] = root;
            flush();
        }
    }

    void flush() {
        ofstream fout(storage::path("accounts.tsv"), ios::trunc);
        for (auto &kv: accounts) {
            const auto &a = kv.second;
            fout << joinTab({a.userId, a.password, to_string(a.privilege), a.username}) << '\n';
        }
    }

    bool exists(const string &uid) const { return accounts.count(uid); }

    const Account* get(const string &uid) const {
        auto it = accounts.find(uid);
        if (it == accounts.end()) return nullptr;
        return &it->second;
    }

    bool add(const Account &a) {
        if (exists(a.userId)) return false;
        accounts[a.userId] = a; flush(); return true;
    }

    bool remove(const string &uid) {
        if (!exists(uid)) return false;
        accounts.erase(uid); flush(); return true;
    }

    bool updatePassword(const string &uid, const string &pw) {
        auto it = accounts.find(uid); if (it==accounts.end()) return false;
        it->second.password = pw; flush(); return true;
    }

  private:
    map<string, Account> accounts; // ordered for stable flush
};

// Book database
class BookDB {
  public:
    BookDB() { load(); }

    void load() {
        books.clear();
        ifstream fin(storage::path("books.tsv"));
        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            auto f = splitTab(line);
            if (f.size() < 6) continue;
            Book b; b.isbn=f[0]; b.name=f[1]; b.author=f[2]; b.keyword=f[3];
            b.price = stold(f[4]); b.stock = stoll(f[5]);
            books[b.isbn] = b;
        }
    }

    void flush() {
        ofstream fout(storage::path("books.tsv"), ios::trunc);
        for (auto &kv: books) {
            const auto &b = kv.second;
            fout << joinTab({b.isbn, b.name, b.author, b.keyword, 
                             [&]{ ostringstream os; os.setf(std::ios::fixed); os<<setprecision(2)<< (double)b.price; return os.str();}(),
                             to_string(b.stock)}) << '\n';
        }
    }

    Book* getOrNull(const string &isbn) {
        auto it = books.find(isbn); if (it==books.end()) return nullptr; return &it->second;
    }

    Book& getOrCreate(const string &isbn) {
        auto it = books.find(isbn);
        if (it==books.end()) {
            Book b; b.isbn = isbn; books[isbn] = b; flush();
        }
        return books[isbn];
    }

    bool isbnExists(const string &isbn) const { return books.count(isbn); }

    vector<Book> listAll() const {
        vector<Book> v; v.reserve(books.size());
        for (auto &kv: books) v.push_back(kv.second);
        sort(v.begin(), v.end(), [](const Book&a, const Book&b){return a.isbn<b.isbn;});
        return v;
    }

  private:
    map<string, Book> books;
};

struct Session {
    vector<Account> stack; // login stack
    vector<string> selectedIsbnStack; // per-login selected book

    int currentPrivilege() const { return stack.empty()?0:stack.back().privilege; }
    string& currentSelected() { 
        static string empty; 
        if (selectedIsbnStack.empty()) return empty; 
        return selectedIsbnStack.back();
    }
};

// Finance database (transaction records)
class FinanceDB {
  public:
    FinanceDB() { load(); }

    void addIncome(long double amount) { transactions.push_back(amount); flush(); }
    void addExpenditure(long double amount) { transactions.push_back(-amount); flush(); }

    // sum last k transactions; if k==-1 sum all
    pair<long double,long double> summarize(long long k) const {
        long double income=0.0L, expend=0.0L; 
        long long n = (long long)transactions.size();
        long long start = (k<0 || k>n) ? 0 : n-k;
        for (long long i=start;i<n;++i) {
            long double x = transactions[i];
            if (x>=0) income += x; else expend += -x;
        }
        return {income, expend};
    }

    long long size() const { return (long long)transactions.size(); }

    void load() {
        transactions.clear();
        ifstream fin(storage::path("finance.tsv"));
        string line; 
        while (getline(fin, line)) {
            if (line.empty()) continue; 
            try { transactions.push_back(stold(line)); } catch(...) {}
        }
    }
    void flush() const {
        ofstream fout(storage::path("finance.tsv"), ios::trunc);
        for (auto x: transactions) {
            ostringstream os; os.setf(std::ios::fixed); os<<setprecision(2)<<(double)x;
            fout << os.str() << '\n';
        }
    }
  private:
    vector<long double> transactions;
};

// Validators according to spec
namespace validate {
    inline bool ascii_visible_no_quotes(const string &s) {
        for (unsigned char c: s) {
            if (c < 32 || c == '"') return false;
        }
        return true;
    }
    inline bool ascii_visible(const string &s) {
        for (unsigned char c: s) if (c < 32) return false; return true;
    }
    inline bool id_or_password(const string &s) {
        if (s.size()>30) return false; if (s.empty()) return false;
        for (unsigned char c: s) if (!(isalnum(c) || c=='_')) return false; return true;
    }
    inline bool username(const string &s) {
        return !s.empty() && s.size()<=30 && ascii_visible(s);
    }
    inline bool privilege(const string &s, int &out) {
        if (s.size()!=1 || !isdigit(s[0])) return false; out = s[0]-'0';
        return (out==7 || out==3 || out==1);
    }
    inline bool isbn(const string &s) {
        return !s.empty() && s.size()<=20 && ascii_visible(s);
    }
    inline bool bookname_or_author(const string &s) {
        return !s.empty() && s.size()<=60 && ascii_visible_no_quotes(s);
    }
    inline bool keyword(const string &s) {
        if (s.empty() || s.size()>60) return false;
        if (!ascii_visible_no_quotes(s)) return false;
        // cannot contain multiple keywords for show; for modify we allow '|' but must handle duplicates later
        return true;
    }
    inline bool quantity(const string &s, long long &out) {
        if (s.empty() || s.size()>10) return false; if (s[0]=='0' && s.size()>1) {
            // allow 0? For buy/import must be positive
        }
        for (char c: s) if (!isdigit(c)) return false; 
        try { 
            long long v = stoll(s); if (v<0 || v>2147483647LL) return false; out = v; return true;
        } catch(...) { return false; }
    }
    inline bool money(const string &s, long double &out) {
        if (s.empty() || s.size()>13) return false;
        int dots=0; for (char c: s){ if (c=='.') dots++; else if(!isdigit(c)) return false; }
        if (dots>1) return false; 
        try { out = stold(s); return true; } catch(...) { return false; }
    }
}

// Parser to handle command with quotes and spaces
static vector<string> parseCommand(const string &line) {
    vector<string> parts; string cur; bool inQuote=false; 
    for (size_t i=0;i<line.size();++i) {
        char c = line[i];
        if (inQuote) {
            if (c=='"') { inQuote=false; }
            else { cur.push_back(c); }
        } else {
            if (isspace((unsigned char)c)) {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else if (c=='"') {
                inQuote=true;
            } else {
                cur.push_back(c);
            }
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

static void printMoney(long double x) {
    ostringstream os; os.setf(std::ios::fixed); os<<setprecision(2)<<(double)x; cout<<os.str() << '\n';
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    AccountDB adb; BookDB bdb; FinanceDB fdb; Session session;

    string line;
    while (true) {
        if (!std::getline(cin, line)) break;
        // trim spaces: extra spaces allowed; parser ignores multiple spaces
        auto tokens = parseCommand(line);
        if (tokens.empty()) continue; // legal: produce no output
        string cmd = tokens[0];
        auto curPriv = [&]() -> int { return session.currentPrivilege(); };

        auto outputInvalid = [&](){ cout << "Invalid\n"; };

        if (cmd=="quit" || cmd=="exit") {
            return 0;
        }
        // show finance must be handled before generic show
        else if (cmd=="show" && tokens.size()>=2 && tokens[1]=="finance") {
            if (curPriv()<7) { outputInvalid(); continue; }
            if (tokens.size()==2) {
                auto [inc, exp] = fdb.summarize(-1);
                ostringstream osi, ose; osi.setf(std::ios::fixed); ose.setf(std::ios::fixed);
                osi<<setprecision(2)<<(double)inc; ose<<setprecision(2)<<(double)exp;
                cout << "+ " << osi.str() << " - " << ose.str() << '\n';
            } else if (tokens.size()==3) {
                long long cnt=0; if (!validate::quantity(tokens[2], cnt)) { outputInvalid(); continue; }
                if (cnt==0) { cout << '\n'; continue; }
                if (cnt > fdb.size()) { outputInvalid(); continue; }
                auto [inc, exp] = fdb.summarize(cnt);
                ostringstream osi, ose; osi.setf(std::ios::fixed); ose.setf(std::ios::fixed);
                osi<<setprecision(2)<<(double)inc; ose<<setprecision(2)<<(double)exp;
                cout << "+ " << osi.str() << " - " << ose.str() << '\n';
            } else { outputInvalid(); }
        }
        else if (cmd=="su") {
            if (!(tokens.size()==2 || tokens.size()==3)) { outputInvalid(); continue; }
            string uid=tokens[1]; string pw = tokens.size()==3?tokens[2]:string();
            if (!validate::id_or_password(uid)) { outputInvalid(); continue; }
            const Account* a = adb.get(uid);
            if (!a) { outputInvalid(); continue; }
            bool canOmit = !session.stack.empty() && session.stack.back().privilege > a->privilege;
            if (pw.empty() && !canOmit) { outputInvalid(); continue; }
            if (!pw.empty() && pw != a->password) { outputInvalid(); continue; }
            session.stack.push_back(*a);
            session.selectedIsbnStack.push_back("");
        }
        else if (cmd=="logout") {
            if (curPriv()<1) { outputInvalid(); continue; }
            if (session.stack.empty()) { outputInvalid(); continue; }
            session.stack.pop_back();
            if (!session.selectedIsbnStack.empty()) session.selectedIsbnStack.pop_back();
        }
        else if (cmd=="register") {
            if (tokens.size()!=4) { outputInvalid(); continue; }
            string uid=tokens[1], pw=tokens[2], uname=tokens[3];
            if (!(validate::id_or_password(uid) && validate::id_or_password(pw) && validate::username(uname))) { outputInvalid(); continue; }
            if (adb.exists(uid)) { outputInvalid(); continue; }
            Account a{uid, pw, 1, uname};
            adb.add(a);
        }
        else if (cmd=="passwd") {
            if (curPriv()<1) { outputInvalid(); continue; }
            if (!(tokens.size()==3 || tokens.size()==4)) { outputInvalid(); continue; }
            string uid=tokens[1]; if (!validate::id_or_password(uid)) { outputInvalid(); continue; }
            const Account* a = adb.get(uid); if (!a) { outputInvalid(); continue; }
            if (curPriv()==7) {
                string newpw = tokens.back(); if (!validate::id_or_password(newpw)) { outputInvalid(); continue; }
                adb.updatePassword(uid, newpw);
            } else {
                if (tokens.size()!=4) { outputInvalid(); continue; }
                string curpw=tokens[2], newpw=tokens[3];
                if (!(validate::id_or_password(curpw) && validate::id_or_password(newpw))) { outputInvalid(); continue; }
                if (curpw != a->password) { outputInvalid(); continue; }
                adb.updatePassword(uid, newpw);
            }
        }
        else if (cmd=="useradd") {
            if (curPriv()<3) { outputInvalid(); continue; }
            if (tokens.size()!=5) { outputInvalid(); continue; }
            string uid=tokens[1], pw=tokens[2], privStr=tokens[3], uname=tokens[4];
            int priv=0; if (!(validate::id_or_password(uid) && validate::id_or_password(pw) && validate::username(uname) && validate::privilege(privStr, priv))) { outputInvalid(); continue; }
            if (priv>=curPriv()) { outputInvalid(); continue; }
            if (adb.exists(uid)) { outputInvalid(); continue; }
            Account a{uid, pw, priv, uname};
            adb.add(a);
        }
        else if (cmd=="delete") {
            if (curPriv()<7) { outputInvalid(); continue; }
            if (tokens.size()!=2) { outputInvalid(); continue; }
            string uid=tokens[1]; if (!validate::id_or_password(uid)) { outputInvalid(); continue; }
            if (!adb.exists(uid)) { outputInvalid(); continue; }
            // cannot delete if logged in
            bool loggedIn=false; for (auto &x: session.stack) if (x.userId==uid) { loggedIn=true; break; }
            if (loggedIn) { outputInvalid(); continue; }
            adb.remove(uid);
        }
        else if (cmd=="show") {
            if (curPriv()<1) { outputInvalid(); continue; }
            // show; show -ISBN=; -name="" etc.
            // parse optional single filter
            string ftype; string fval; 
            if (tokens.size()>1) {
                if (tokens.size()!=2) { outputInvalid(); continue; }
                string t = tokens[1];
                auto pos = t.find('='); if (pos==string::npos) { outputInvalid(); continue; }
                ftype = t.substr(0, pos); fval = t.substr(pos+1);
                if (ftype=="-ISBN") { if (!validate::isbn(fval)) { outputInvalid(); continue; } }
                else if (ftype=="-name") { if (!validate::bookname_or_author(fval)) { outputInvalid(); continue; } }
                else if (ftype=="-author") { if (!validate::bookname_or_author(fval)) { outputInvalid(); continue; } }
                else if (ftype=="-keyword") { 
                    if (!validate::keyword(fval)) { outputInvalid(); continue; }
                    if (fval.find('|')!=string::npos) { outputInvalid(); continue; }
                } else { outputInvalid(); continue; }
            }
            vector<Book> all = bdb.listAll();
            bool firstLine=true; 
            for (const auto &b: all) {
                bool ok=true;
                if (!ftype.empty()) {
                    if (ftype=="-ISBN") ok = (b.isbn==fval);
                    else if (ftype=="-name") ok = (b.name==fval);
                    else if (ftype=="-author") ok = (b.author==fval);
                    else if (ftype=="-keyword") {
                        // keywords are '|' separated; unordered; but show filter cannot be multiple keywords
                        string kw = b.keyword;
                        // match if any segment equals fval
                        bool hit=false; string seg; 
                        for (size_t i=0;i<=kw.size();++i) {
                            if (i==kw.size() || kw[i]=='|') {
                                if (!seg.empty() && seg==fval) { hit=true; break; }
                                seg.clear();
                            } else seg.push_back(kw[i]);
                        }
                        ok = hit;
                    }
                }
                if (ok) {
                    if (!firstLine) cout << '\n';
                    firstLine=false;
                    ostringstream os; os.setf(std::ios::fixed); os<<setprecision(2)<<(double)b.price;
                    cout << b.isbn << '\t' << b.name << '\t' << b.author << '\t' << b.keyword << '\t' << os.str() << '\t' << b.stock;
                }
            }
            if (!firstLine) cout << '\n';
            else cout << '\n'; // empty line when no books
        }
        else if (cmd=="buy") {
            if (curPriv()<1) { outputInvalid(); continue; }
            if (tokens.size()!=3) { outputInvalid(); continue; }
            string isbn=tokens[1]; long long qty=0; if (!validate::isbn(isbn) || !validate::quantity(tokens[2], qty) || qty<=0) { outputInvalid(); continue; }
            Book* b = bdb.getOrNull(isbn); if (!b) { outputInvalid(); continue; }
            if (b->stock < qty) { outputInvalid(); continue; }
            b->stock -= qty; long double cost = (long double)qty * b->price; bdb.flush();
            fdb.addIncome(cost);
            printMoney(cost);
        }
        else if (cmd=="select") {
            if (curPriv()<3) { outputInvalid(); continue; }
            if (tokens.size()!=2) { outputInvalid(); continue; }
            string isbn=tokens[1]; if (!validate::isbn(isbn)) { outputInvalid(); continue; }
            // create if not exist
            bdb.getOrCreate(isbn);
            session.currentSelected() = isbn;
        }
        else if (cmd=="modify") {
            if (curPriv()<3) { outputInvalid(); continue; }
            if (tokens.size()<2) { outputInvalid(); continue; }
            if (session.currentSelected().empty()) { outputInvalid(); continue; }
            // no duplicate flags
            set<string> seen;
            Book* b = bdb.getOrNull(session.currentSelected()); if (!b) { outputInvalid(); continue; }
            string newISBN=b->isbn, newName=b->name, newAuthor=b->author, newKeyword=b->keyword; long double newPrice=b->price;
            for (size_t i=1;i<tokens.size();++i) {
                string t = tokens[i]; auto pos=t.find('='); if (pos==string::npos) { outputInvalid(); goto nextline; }
                string k=t.substr(0,pos), v=t.substr(pos+1);
                if (seen.count(k)) { outputInvalid(); goto nextline; }
                seen.insert(k);
                if (k=="-ISBN") { if (!validate::isbn(v)) { outputInvalid(); goto nextline; } if (v==b->isbn) { outputInvalid(); goto nextline; } if (v!=b->isbn && bdb.isbnExists(v)) { outputInvalid(); goto nextline; } newISBN=v; }
                else if (k=="-name") { if (!validate::bookname_or_author(v)) { outputInvalid(); goto nextline; } newName=v; }
                else if (k=="-author") { if (!validate::bookname_or_author(v)) { outputInvalid(); goto nextline; } newAuthor=v; }
                else if (k=="-keyword") {
                    if (!validate::keyword(v)) { outputInvalid(); goto nextline; }
                    // no duplicate segments
                    set<string> segs; string seg; bool dup=false; for (size_t j=0;j<=v.size();++j){ if (j==v.size()||v[j]=='|'){ if (seg.empty()||segs.count(seg)){ dup=true; break;} segs.insert(seg); seg.clear(); } else seg.push_back(v[j]); }
                    if (dup) { outputInvalid(); goto nextline; }
                    newKeyword=v;
                }
                else if (k=="-price") { long double m; if (!validate::money(v, m)) { outputInvalid(); goto nextline; } newPrice=m; }
                else { outputInvalid(); goto nextline; }
            }
            {
                // apply
                if (newISBN!=b->isbn) {
                    // move entry in map
                    Book nb = *b; nb.isbn=newISBN; nb.name=newName; nb.author=newAuthor; nb.keyword=newKeyword; nb.price=newPrice;
                    // remove old and insert new
                    *b = nb; // temp update to flush content
                    // Actually we will rebuild map by load after flush
                    bdb.flush();
                    // reload to rebuild keys properly
                    bdb.load();
                    session.currentSelected() = newISBN;
                } else {
                    b->name=newName; b->author=newAuthor; b->keyword=newKeyword; b->price=newPrice; bdb.flush();
                }
            }
        nextline:
            ;
        }
        else if (cmd=="import") {
            if (curPriv()<3) { outputInvalid(); continue; }
            if (tokens.size()!=3) { outputInvalid(); continue; }
            if (session.currentSelected().empty()) { outputInvalid(); continue; }
            long long qty=0; long double total=0; if (!validate::quantity(tokens[1], qty) || !validate::money(tokens[2], total) || qty<=0 || total<=0) { outputInvalid(); continue; }
            Book* b = bdb.getOrNull(session.currentSelected()); if (!b) { outputInvalid(); continue; }
            b->stock += qty; bdb.flush();
            fdb.addExpenditure(total);
        }
        else if (cmd=="log" || cmd=="report") {
            if (curPriv()<7) { outputInvalid(); continue; }
            // Placeholder: produce empty output
            cout << '\n';
        }
        else {
            outputInvalid();
        }
    }
    return 0;
}
