/*
 * DIGITAL LOST & FOUND  -  Modern Windows GUI (Win32 + GDI, C++)
 * -----------------------------------------------------------------
 * Works in Dev-C++ 5.11 (C++98 style, no extra libraries or linker flags needed).
 * Everything you see (cards, buttons, table, badges) is custom drawn with GDI.
 *
 * Experiment mapping:
 *  Exp 1  Frequency counting  -> letter/digit frequency "Smart Match", category histogram
 *  Exp 2  Variadic functions  -> sumOf(), minOf(), maxOf() (stat cards + Statistics)
 *  Exp 3  Pointers            -> SetValue(int*, int*) used in Statistics
 *  Exp 4  Classes             -> Item class
 *  Exp 5  Stack / Queue       -> Undo stack, Claim request queue
 *  Exp 6  Linked list         -> Item list + "Smart Clean" (friend-deletion algorithm)
 */

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
using namespace std;

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

const char* CATEGORIES[5]     = {"Electronics", "Documents", "Accessories", "Clothing", "Others"};
const char* PRIORITY_NAMES[4] = {"", "Low", "Medium", "High"};
const char* DATA_FILE         = "lostfound_data.txt";

enum Status { LOST = 0, FOUND = 1, CLAIMED = 2 };

const char* statusName(Status s) {
    if (s == LOST) return "LOST";
    if (s == FOUND) return "FOUND";
    return "CLAIMED";
}

// =================================================================
// EXPERIMENT 2 : Variadic functions (first argument = count)
// =================================================================
int sumOf(int count, ...) {
    va_list ap; va_start(ap, count);
    int s = 0;
    for (int i = 0; i < count; i++) s += va_arg(ap, int);
    va_end(ap);
    return s;
}
int minOf(int count, ...) {
    va_list ap; va_start(ap, count);
    int m = va_arg(ap, int);
    for (int i = 1; i < count; i++) { int v = va_arg(ap, int); if (v < m) m = v; }
    va_end(ap);
    return m;
}
int maxOf(int count, ...) {
    va_list ap; va_start(ap, count);
    int m = va_arg(ap, int);
    for (int i = 1; i < count; i++) { int v = va_arg(ap, int); if (v > m) m = v; }
    va_end(ap);
    return m;
}

// =================================================================
// EXPERIMENT 3 : Pointers  (a = a + b, b = |a - b|)
// =================================================================
void SetValue(int *a, int *b) {
    int sum  = *a + *b;
    int diff = (*a > *b) ? (*a - *b) : (*b - *a);
    *a = sum;
    *b = diff;
}

// ---------- helpers ----------
string num(int v) { ostringstream o; o << v; return o.str(); }

string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

string clean(string s) {                       // trim + remove the file delimiter
    for (size_t i = 0; i < s.size(); i++) if (s[i] == '|') s[i] = '/';
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// =================================================================
// EXPERIMENT 4 : Class
// =================================================================
class Item {
public:
    int id;
    string name, location, date, contact;
    int catIdx;       // 0..4
    int priority;     // 1 = Low, 2 = Medium, 3 = High (value of the item)
    Status status;

    Item() : id(0), catIdx(4), priority(1), status(LOST) {}

    string details() const {
        ostringstream o;
        o << "ID       : " << id << "\n"
          << "Name     : " << name << "\n"
          << "Category : " << CATEGORIES[catIdx] << "\n"
          << "Location : " << location << "\n"
          << "Date     : " << date << "\n"
          << "Contact  : " << contact << "\n"
          << "Value    : " << PRIORITY_NAMES[priority] << "\n"
          << "Status   : " << statusName(status) << "\n";
        return o.str();
    }

    string serialize() const {
        ostringstream ss;
        ss << id << '|' << name << '|' << catIdx << '|' << location << '|' << date << '|'
           << contact << '|' << (int)status << '|' << priority;
        return ss.str();
    }

    bool deserialize(const string& s) {
        istringstream ss(s);
        string f[8];
        for (int i = 0; i < 8; i++) if (!getline(ss, f[i], '|')) return false;
        id = atoi(f[0].c_str()); name = f[1]; catIdx = atoi(f[2].c_str());
        location = f[3]; date = f[4]; contact = f[5];
        status = (Status)atoi(f[6].c_str()); priority = atoi(f[7].c_str());
        if (catIdx < 0 || catIdx > 4 || priority < 1 || priority > 3) return false;
        return true;
    }
};

// =================================================================
// EXPERIMENT 6 : Linked list of items
// =================================================================
struct Node {
    Item data;
    Node* next;
    Node(const Item& d) : data(d), next(NULL) {}
};

class ItemList {
public:
    Node* head;
    int count;

    ItemList() : head(NULL), count(0) {}
    ~ItemList() { while (head) { Node* t = head; head = head->next; delete t; } }

    void addEnd(const Item& it) {
        Node* n = new Node(it);
        if (!head) head = n;
        else { Node* c = head; while (c->next) c = c->next; c->next = n; }
        count++;
    }

    Node* find(int id) {
        for (Node* c = head; c; c = c->next) if (c->data.id == id) return c;
        return NULL;
    }

    bool removeById(int id, Item* out) {
        Node *prev = NULL, *cur = head;
        while (cur && cur->data.id != id) { prev = cur; cur = cur->next; }
        if (!cur) return false;
        if (out) *out = cur->data;
        if (prev) prev->next = cur->next; else head = cur->next;
        delete cur;
        count--;
        return true;
    }

    // Exp 6 algorithm (Christie's friend deletion) applied to item value:
    // delete the first item whose value is lower than the NEXT item's value;
    // if no such item exists, delete the last item.
    bool smartCleanOnce(Item* removed) {
        if (!head) return false;
        Node *prev = NULL, *cur = head;
        while (cur->next) {
            if (cur->data.priority < cur->next->data.priority) break;
            prev = cur; cur = cur->next;
        }
        if (removed) *removed = cur->data;
        if (prev) prev->next = cur->next; else head = cur->next;
        delete cur;
        count--;
        return true;
    }

    int maxId() {
        int m = 0;
        for (Node* c = head; c; c = c->next) if (c->data.id > m) m = c->data.id;
        return m;
    }
};

// =================================================================
// EXPERIMENT 5 : Stack (undo) and Queue (claims)
// =================================================================
struct Action {
    int type;       // 0 = item was added, 1 = item was deleted
    Item item;
};

struct SNode { Action a; SNode* next; SNode(const Action& x) : a(x), next(NULL) {} };

class UndoStack {
    SNode* top;
public:
    UndoStack() : top(NULL) {}
    ~UndoStack() { Action tmp; while (pop(&tmp)) {} }
    void push(int type, const Item& it) {
        Action a; a.type = type; a.item = it;
        SNode* n = new SNode(a);
        n->next = top; top = n;
    }
    bool pop(Action* out) {
        if (!top) return false;
        SNode* t = top; *out = t->a; top = t->next; delete t;
        return true;
    }
};

struct Claim { int itemId; string claimant, contact; };
struct QNode { Claim c; QNode* next; QNode(const Claim& x) : c(x), next(NULL) {} };

class ClaimQueue {
    QNode *front, *rear;
public:
    int size;
    ClaimQueue() : front(NULL), rear(NULL), size(0) {}
    ~ClaimQueue() { Claim tmp; while (dequeue(&tmp)) {} }
    void enqueue(const Claim& c) {
        QNode* n = new QNode(c);
        if (!rear) front = rear = n; else { rear->next = n; rear = n; }
        size++;
    }
    bool dequeue(Claim* out) {
        if (!front) return false;
        QNode* t = front; *out = t->c; front = t->next;
        if (!front) rear = NULL;
        delete t; size--;
        return true;
    }
    string toText() const {
        ostringstream o;
        int pos = 1;
        for (QNode* c = front; c; c = c->next, pos++)
            o << "#" << pos << "  Item ID " << c->c.itemId << "  |  " << c->c.claimant
              << "  |  " << c->c.contact << "\n";
        return o.str();
    }
};

// =================================================================
// EXPERIMENT 1 : Frequency counting (letters + digits = 36 slots)
// =================================================================
void freqOf(const string& s, int f[36]) {
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (isalpha((unsigned char)c)) f[tolower((unsigned char)c) - 'a']++;
        else if (isdigit((unsigned char)c)) f[26 + (c - '0')]++;
    }
}

// Returns 0..100 : how likely a lost item and a found item are the same object
int matchScore(const Item& a, const Item& b) {
    int fa[36] = {0}, fb[36] = {0};
    freqOf(a.name, fa); freqOf(b.name, fb);
    int common = 0, la = 0, lb = 0;
    for (int i = 0; i < 36; i++) { common += min(fa[i], fb[i]); la += fa[i]; lb += fb[i]; }
    int nameSim = (la + lb) ? (common * 200) / (la + lb) : 0;       // 0..100
    int score = (nameSim * 70) / 100;                               // name = 70%
    if (a.catIdx == b.catIdx) score += 20;                          // category = 20%
    if (toLower(a.location) == toLower(b.location)) score += 10;    // location = 10%
    return score;
}

// =================================================================
// APPLICATION DATA
// =================================================================
ItemList   items;
UndoStack  undoStack;
ClaimQueue claims;
int        nextId = 1;
string     filterText = "";
int        shownCount = 0;

int curType = 0;     // 0 = lost, 1 = found
int curCat  = 0;     // 0..4
int curPri  = 1;     // 1..3

void saveData() {
    ofstream out(DATA_FILE);
    for (Node* c = items.head; c; c = c->next) out << c->data.serialize() << "\n";
}

void loadData() {
    ifstream in(DATA_FILE);
    if (!in) return;
    string s;
    while (getline(in, s)) {
        Item it;
        if (it.deserialize(s)) items.addEnd(it);
    }
    nextId = items.maxId() + 1;
}

void calcStats(int st[3], int cat[5]) {
    for (int i = 0; i < 3; i++) st[i] = 0;
    for (int i = 0; i < 5; i++) cat[i] = 0;
    for (Node* c = items.head; c; c = c->next) { st[c->data.status]++; cat[c->data.catIdx]++; }
}

// =================================================================
// GUI : colours, fonts, ids
// =================================================================
#define C_BG        RGB(240, 243, 250)
#define C_CARD      RGB(255, 255, 255)
#define C_LINE      RGB(226, 232, 240)
#define C_FIELD     RGB(246, 248, 252)
#define C_FIELDLINE RGB(203, 213, 225)
#define C_DARK      RGB(30, 41, 59)
#define C_MUTED     RGB(100, 116, 139)
#define C_BLUE      RGB(37, 99, 235)
#define C_RED       RGB(239, 68, 68)
#define C_GREEN     RGB(34, 197, 94)
#define C_AMBER     RGB(245, 158, 11)

enum {
    ID_TLOST = 100, ID_TFOUND, ID_CAT0, ID_CAT1, ID_CAT2, ID_CAT3, ID_CAT4,
    ID_PLOW, ID_PMED, ID_PHIGH,
    ID_ADD, ID_SUBMITCLAIM, ID_PROCESSCLAIM, ID_VIEWCLAIMS, ID_CLEAN,
    ID_BTNSEARCH, ID_SHOWALL, ID_DELETE, ID_UNDO, ID_MATCH, ID_STATS,
    ID_BTN_LAST = ID_STATS,
    ID_NAME = 200, ID_LOC, ID_DATE, ID_CONTACT, ID_CLAIMANT, ID_CLAIMCONTACT,
    ID_K, ID_SEARCH, ID_LIST, ID_OUT
};

HWND hWnd, hName, hLoc, hDate, hContact, hClaimant, hClaimContact, hK, hSearch, hList, hOut;
HFONT hUI, hBold, hSmallB, hCardTitle, hTitleF, hBig, hMono;
HBRUSH hBrField;
int hoverId = 0;

const int LIST_X = 392;      // table position (used by painting and row drawing)
const int ROW_H  = 36;

struct Lbl { int x, y, kind; const char* text; };
Lbl lbls[40];  int nLbl = 0;
RECT frames[16]; int nFrames = 0;

void addLbl(int x, int y, int kind, const char* t) {
    lbls[nLbl].x = x; lbls[nLbl].y = y; lbls[nLbl].kind = kind; lbls[nLbl].text = t; nLbl++;
}
void addFrame(int x, int y, int w, int h) {
    frames[nFrames].left = x; frames[nFrames].top = y;
    frames[nFrames].right = x + w; frames[nFrames].bottom = y + h; nFrames++;
}

// ---------- drawing helpers ----------
void fillRc(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r = {x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void rrect(HDC dc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, x, y, x + w + 1, y + h + 1, r * 2, r * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void circle(HDC dc, int x, int y, int d, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    Ellipse(dc, x, y, x + d, y + d);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void txt(HDC dc, HFONT f, COLORREF c, int x, int y, int w, int h, const string& s, UINT flags) {
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    RECT r = {x, y, x + w, y + h};
    DrawTextA(dc, s.c_str(), -1, &r, flags | DT_NOPREFIX);
    SelectObject(dc, of);
}

void gradient(HDC dc, int x, int y, int w, int h, COLORREF a, COLORREF b) {
    for (int i = 0; i < w; i += 3) {
        int t = (i * 255) / w;
        int rr = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 255;
        int gg = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 255;
        int bb = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 255;
        fillRc(dc, x + i, y, 3, h, RGB(rr, gg, bb));
    }
}

void magnifier(HDC dc, int x, int y, int size, int pw, COLORREF col) {
    HPEN p = CreatePen(PS_SOLID, pw, col);
    HGDIOBJ op = SelectObject(dc, p);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, x, y, x + size, y + size);
    MoveToEx(dc, x + size - size / 6, y + size - size / 6, NULL);
    LineTo(dc, x + size + size / 3, y + size + size / 3);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(p);
}

COLORREF shade(COLORREF c, int amt) {
    int r = GetRValue(c) + amt, g = GetGValue(c) + amt, b = GetBValue(c) + amt;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

void card(HDC dc, int x, int y, int w, int h) {
    rrect(dc, x, y + 2, w, h, 14, RGB(225, 230, 240), RGB(225, 230, 240));   // soft shadow
    rrect(dc, x, y, w, h, 14, C_CARD, C_LINE);
}

void statCard(HDC dc, int x, int y, int w, int h, COLORREF accent, const char* glyph,
              const string& number, const char* label) {
    card(dc, x, y, w, h);
    circle(dc, x + 18, y + 16, 44, accent, accent);
    txt(dc, hBig, RGB(255, 255, 255), x + 18, y + 16, 44, 44, glyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hBig, C_DARK, x + 76, y + 8, w - 90, 38, number, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hUI, C_MUTED, x + 76, y + 46, w - 90, 20, label, DT_LEFT | DT_SINGLELINE);
}

void pill(HDC dc, int x, int y, int w, int h, COLORREF bg, COLORREF fg, const char* text) {
    rrect(dc, x, y, w, h, h / 2, bg, bg);
    txt(dc, hSmallB, fg, x, y, w, h, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------- the whole window background ----------
void paintAll(HDC dc, int W, int H) {
    fillRc(dc, 0, 0, W, H, C_BG);

    // ===== header =====
    gradient(dc, 0, 0, W, 76, RGB(30, 58, 138), RGB(59, 130, 246));
    rrect(dc, 20, 14, 48, 48, 12, RGB(255, 255, 255), RGB(255, 255, 255));
    magnifier(dc, 31, 24, 18, 4, C_BLUE);
    txt(dc, hTitleF, RGB(255, 255, 255), 82, 8, 700, 38, "Digital Lost & Found", DT_LEFT | DT_SINGLELINE);
    txt(dc, hUI, RGB(205, 224, 255), 84, 46, 700, 20, "Smart item recovery  |  Report  -  Match  -  Claim", DT_LEFT | DT_SINGLELINE);
    pill(dc, 936, 24, 104, 28, RGB(255, 255, 255), C_BLUE, "Linked List");
    pill(dc, 1050, 24, 80, 28, RGB(255, 255, 255), C_BLUE, "Stack");
    pill(dc, 1140, 24, 80, 28, RGB(255, 255, 255), C_BLUE, "Queue");

    // ===== stat cards =====
    int st[3], cat[5];
    calcStats(st, cat);
    int total = sumOf(3, st[LOST], st[FOUND], st[CLAIMED]);              // Exp 2
    statCard(dc, 20,  90, 288, 76, C_BLUE,  "#", num(total),       "Total items");
    statCard(dc, 324, 90, 288, 76, C_RED,   "?", num(st[LOST]),    "Lost items");
    statCard(dc, 628, 90, 288, 76, C_GREEN, "!", num(st[FOUND]),   "Found items");
    statCard(dc, 932, 90, 288, 76, C_AMBER, "+", num(st[CLAIMED]), "Claimed items");

    // ===== two big cards =====
    card(dc, 20, 182, 340, 558);
    card(dc, 376, 182, 844, 558);

    // ===== labels and titles =====
    for (int i = 0; i < nLbl; i++) {
        if (lbls[i].kind == 1)
            txt(dc, hCardTitle, C_DARK, lbls[i].x, lbls[i].y, 300, 22, lbls[i].text, DT_LEFT | DT_SINGLELINE);
        else
            txt(dc, hSmallB, C_MUTED, lbls[i].x, lbls[i].y, 200, 16, lbls[i].text, DT_LEFT | DT_SINGLELINE);
    }

    // ===== text field frames =====
    for (int i = 0; i < nFrames; i++)
        rrect(dc, frames[i].left, frames[i].top, frames[i].right - frames[i].left,
              frames[i].bottom - frames[i].top, 8, C_FIELD, C_FIELDLINE);

    // search icon inside the search box
    magnifier(dc, 404, 207, 10, 2, C_MUTED);

    // "Showing x of y"
    string cnt = "Showing " + num(shownCount) + " of " + num(items.count);
    txt(dc, hUI, C_MUTED, 1030, 206, 174, 20, cnt, DT_RIGHT | DT_SINGLELINE);

    // ===== table header strip =====
    fillRc(dc, LIST_X, 244, 812, 28, RGB(241, 245, 249));
    txt(dc, hSmallB, C_MUTED, LIST_X + 14,  244, 40,  28, "ID",       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 56,  244, 180, 28, "ITEM",     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 250, 244, 110, 28, "CATEGORY", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 372, 244, 170, 28, "LOCATION", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 556, 244, 90,  28, "DATE",     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 654, 244, 70,  28, "VALUE",    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(dc, hSmallB, C_MUTED, LIST_X + 730, 244, 80,  28, "STATUS",   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ===== tip box (left card bottom) =====
    rrect(dc, 36, 650, 308, 74, 10, RGB(239, 246, 255), RGB(191, 219, 254));
    txt(dc, hUI, RGB(30, 64, 175), 48, 658, 286, 60,
        "Tip: click a row in the table to see its details. Smart Match pairs lost items with found ones, and Undo reverses your last add or delete.",
        DT_LEFT | DT_WORDBREAK);

    // ===== activity frame =====
    rrect(dc, 392, 570, 480, 156, 8, C_FIELD, C_FIELDLINE);
}

// ---------- button colours ----------
void btnLook(int id, COLORREF* fill, COLORREF* textc, COLORREF* border) {
    COLORREF white = RGB(255, 255, 255);
    *fill = white; *textc = C_MUTED; *border = C_FIELDLINE;      // default: un-selected toggle look

    if (id == ID_TLOST)  { if (curType == 0) { *fill = C_RED;   *textc = white; *border = C_RED; } return; }
    if (id == ID_TFOUND) { if (curType == 1) { *fill = C_GREEN; *textc = white; *border = C_GREEN; } return; }
    if (id >= ID_CAT0 && id <= ID_CAT4) {
        if (curCat == id - ID_CAT0) { *fill = C_BLUE; *textc = white; *border = C_BLUE; }
        return;
    }
    if (id >= ID_PLOW && id <= ID_PHIGH) {
        int p = id - ID_PLOW + 1;
        if (curPri == p) {
            COLORREF c = (p == 1) ? RGB(100, 116, 139) : (p == 2) ? C_AMBER : C_RED;
            *fill = c; *textc = white; *border = c;
        }
        return;
    }

    COLORREF c = C_BLUE;
    switch (id) {
        case ID_ADD:          c = C_BLUE;             break;
        case ID_BTNSEARCH:    c = C_BLUE;             break;
        case ID_SUBMITCLAIM:  c = RGB(79, 70, 229);   break;
        case ID_PROCESSCLAIM: c = RGB(22, 163, 74);   break;
        case ID_CLEAN:        c = RGB(217, 119, 6);   break;
        case ID_DELETE:       c = RGB(220, 38, 38);   break;
        case ID_UNDO:         c = RGB(71, 85, 105);   break;
        case ID_MATCH:        c = RGB(124, 58, 237);  break;
        case ID_STATS:        c = RGB(13, 148, 136);  break;
        case ID_VIEWCLAIMS:   *fill = white; *textc = C_BLUE; *border = C_BLUE; return;
        case ID_SHOWALL:      *fill = white; *textc = C_DARK; *border = C_FIELDLINE; return;
    }
    *fill = c; *textc = white; *border = c;
}

void drawButton(DRAWITEMSTRUCT* d) {
    int id = (int)d->CtlID;
    HDC dc = d->hDC;
    RECT rc = d->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    fillRc(dc, rc.left, rc.top, w, h, C_CARD);        // card colour behind the rounded corners

    COLORREF fill, textc, border;
    btnLook(id, &fill, &textc, &border);

    bool pressed = (d->itemState & ODS_SELECTED) != 0;
    bool hover = (hoverId == id);
    if (pressed)    fill = shade(fill, -38);
    else if (hover) fill = shade(fill, -16);

    rrect(dc, rc.left, rc.top, w - 1, h - 1, 8, fill, border);

    char buf[100];
    GetWindowTextA(d->hwndItem, buf, sizeof(buf));
    txt(dc, hBold, textc, rc.left, rc.top + (pressed ? 1 : 0), w, h, buf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------- table rows ----------
void drawRow(DRAWITEMSTRUCT* d) {
    if (d->itemID == (UINT)-1) return;
    HDC dc = d->hDC;
    RECT rc = d->rcItem;
    Node* n = items.find((int)d->itemData);
    if (!n) return;
    const Item& it = n->data;

    bool sel = (d->itemState & ODS_SELECTED) != 0;
    COLORREF bg = sel ? RGB(239, 246, 255) : ((d->itemID % 2) ? RGB(250, 251, 253) : RGB(255, 255, 255));
    int x = rc.left, y = rc.top, h = rc.bottom - rc.top;
    fillRc(dc, x, y, rc.right - rc.left, h, bg);
    fillRc(dc, x, rc.bottom - 1, rc.right - rc.left, 1, RGB(237, 241, 247));
    if (sel) fillRc(dc, x, y, 4, h, C_BLUE);

    UINT f = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    txt(dc, hUI,   C_MUTED,           x + 14,  y, 36,  h, num(it.id),            f);
    txt(dc, hBold, C_DARK,            x + 56,  y, 186, h, it.name,               f);
    txt(dc, hUI,   RGB(71, 85, 105),  x + 250, y, 116, h, CATEGORIES[it.catIdx], f);
    txt(dc, hUI,   RGB(71, 85, 105),  x + 372, y, 178, h, it.location,           f);
    txt(dc, hUI,   RGB(71, 85, 105),  x + 556, y, 94,  h, it.date,               f);

    // value dots
    COLORREF dot = (it.priority == 1) ? RGB(100, 116, 139) : (it.priority == 2) ? C_AMBER : C_RED;
    for (int i = 0; i < 3; i++) {
        int cx = x + 656 + i * 16, cy = y + (h - 10) / 2;
        if (i < it.priority) circle(dc, cx, cy, 10, dot, dot);
        else                 circle(dc, cx, cy, 10, RGB(226, 232, 240), RGB(226, 232, 240));
    }

    // status badge
    int py = y + (h - 22) / 2;
    if (it.status == LOST)         pill(dc, x + 730, py, 74, 22, RGB(254, 226, 226), RGB(185, 28, 28),  "LOST");
    else if (it.status == FOUND)   pill(dc, x + 730, py, 74, 22, RGB(220, 252, 231), RGB(21, 128, 61),  "FOUND");
    else                           pill(dc, x + 730, py, 74, 22, RGB(254, 243, 199), RGB(180, 83, 9),   "CLAIMED");
}

// ---------- controls ----------
HWND mk(const char* cls, const char* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                             hWnd, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)hUI, TRUE);
    return c;
}

HWND btn(const char* text, int x, int y, int w, int h, int id) {
    return mk("BUTTON", text, WS_TABSTOP | BS_OWNERDRAW, x, y, w, h, id);
}

HWND field(int x, int y, int w, int id, int indent) {
    addFrame(x, y, w, 30);
    return mk("EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, x + 10 + indent, y + 6, w - 20 - indent, 20, id);
}

string getText(HWND h) {
    char buf[512];
    GetWindowTextA(h, buf, sizeof(buf));
    return clean(buf);
}

void setOut(const string& s) {
    string t;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\n') t += "\r\n"; else t += s[i];
    }
    SetWindowTextA(hOut, t.c_str());
}

int selectedId() {
    int idx = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
    if (idx == LB_ERR) return -1;
    return (int)SendMessageA(hList, LB_GETITEMDATA, idx, 0);
}

void redrawCtl(int id) {
    HWND c = GetDlgItem(hWnd, id);
    if (c) InvalidateRect(c, NULL, FALSE);
}
void redrawGroup(int a, int b) { for (int i = a; i <= b; i++) redrawCtl(i); }

void refreshList() {
    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    string key = toLower(filterText);
    shownCount = 0;
    for (Node* c = items.head; c; c = c->next) {
        if (!key.empty() &&
            toLower(c->data.name).find(key) == string::npos &&
            toLower(c->data.location).find(key) == string::npos) continue;
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)c->data.id);   // item data = the item id
        shownCount++;
    }
    InvalidateRect(hWnd, NULL, FALSE);      // repaint stat cards and counters
}

void changed() { saveData(); refreshList(); }

string todayText() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char buf[32];
    sprintf(buf, "%02d-%02d-%04d", (int)t.wDay, (int)t.wMonth, (int)t.wYear);
    return buf;
}

// ---------- actions ----------
void onAdd() {
    string name = getText(hName);
    if (name.empty()) { MessageBoxA(hWnd, "Please enter the item name.", "Missing data", MB_ICONWARNING); return; }

    Item it;
    it.id       = nextId++;
    it.name     = name;
    it.catIdx   = curCat;
    it.location = getText(hLoc);
    it.date     = getText(hDate);
    it.contact  = getText(hContact);
    it.priority = curPri;
    it.status   = (curType == 1) ? FOUND : LOST;

    items.addEnd(it);
    undoStack.push(0, it);

    ostringstream o;
    o << "Saved! Item ID " << it.id << " (" << statusName(it.status) << ").\n";

    Status other = (it.status == LOST) ? FOUND : LOST;
    bool header = false;
    for (Node* c = items.head; c; c = c->next) {
        if (c->data.status != other) continue;
        int sc = (it.status == LOST) ? matchScore(it, c->data) : matchScore(c->data, it);
        if (sc >= 60) {
            if (!header) { o << "\nPossible matches:\n"; header = true; }
            o << "  -> ID " << c->data.id << "  " << c->data.name << "  (" << sc << "% match)\n";
        }
    }
    setOut(o.str());

    SetWindowTextA(hName, ""); SetWindowTextA(hLoc, "");
    SetWindowTextA(hDate, todayText().c_str()); SetWindowTextA(hContact, "");
    changed();
}

void onDelete() {
    int id = selectedId();
    if (id < 0) { MessageBoxA(hWnd, "Select an item in the table first.", "Delete", MB_ICONINFORMATION); return; }
    Item removed;
    if (items.removeById(id, &removed)) {
        undoStack.push(1, removed);
        setOut("Deleted \"" + removed.name + "\".\nUse Undo to bring it back.");
        changed();
    }
}

void onUndo() {
    Action a;
    if (!undoStack.pop(&a)) { setOut("Nothing to undo."); return; }
    if (a.type == 0) {
        items.removeById(a.item.id, NULL);
        setOut("Undid ADD of \"" + a.item.name + "\".");
    } else {
        items.addEnd(a.item);
        setOut("Restored \"" + a.item.name + "\".");
    }
    changed();
}

void onMatch() {
    ostringstream o;
    o << "SMART MATCH (lost vs found)\n---------------------------\n";
    bool any = false;
    for (Node* l = items.head; l; l = l->next) {
        if (l->data.status != LOST) continue;
        for (Node* f = items.head; f; f = f->next) {
            if (f->data.status != FOUND) continue;
            int sc = matchScore(l->data, f->data);
            if (sc >= 50) {
                any = true;
                o << "LOST #" << l->data.id << " \"" << l->data.name << "\"  <->  FOUND #"
                  << f->data.id << " \"" << f->data.name << "\"   " << sc << "%\n";
            }
        }
    }
    if (!any) o << "No likely matches right now.\n";
    setOut(o.str());
}

void onSubmitClaim() {
    int id = selectedId();
    Node* n = (id < 0) ? NULL : items.find(id);
    if (!n || n->data.status != FOUND) {
        MessageBoxA(hWnd, "Select an unclaimed FOUND item in the table first.", "Claim", MB_ICONINFORMATION);
        return;
    }
    Claim c;
    c.itemId   = id;
    c.claimant = getText(hClaimant);
    c.contact  = getText(hClaimContact);
    if (c.claimant.empty()) { MessageBoxA(hWnd, "Enter the claimant's name.", "Claim", MB_ICONWARNING); return; }
    claims.enqueue(c);
    setOut("Claim submitted by " + c.claimant + ".\nPosition in queue: " + num(claims.size));
    SetWindowTextA(hClaimant, ""); SetWindowTextA(hClaimContact, "");
}

void onProcessClaim() {
    Claim c;
    if (!claims.dequeue(&c)) { setOut("No pending claims."); return; }
    Node* n = items.find(c.itemId);
    if (!n || n->data.status != FOUND) {
        setOut("The item is no longer available. Claim discarded.");
        return;
    }
    string msg = "Claimant: " + c.claimant + " (" + c.contact + ")\n\n" + n->data.details() +
                 "\nIs the owner verified? Approve this claim?";
    if (MessageBoxA(hWnd, msg.c_str(), "Process next claim (first come, first served)",
                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
        n->data.status = CLAIMED;
        setOut("Approved. \"" + n->data.name + "\" marked as CLAIMED.");
        changed();
    } else {
        setOut("Claim rejected.");
    }
}

void onViewClaims() {
    if (claims.size == 0) setOut("No pending claims.");
    else setOut("PENDING CLAIMS (front of queue first)\n------------------------------------\n" + claims.toText());
}

void onClean() {
    if (items.count == 0) { setOut("Nothing to clean."); return; }
    int k = atoi(getText(hK).c_str());
    if (k < 1 || k > items.count) {
        MessageBoxA(hWnd, ("Enter K between 1 and " + num(items.count) + ".").c_str(), "Smart Clean", MB_ICONWARNING);
        return;
    }
    ostringstream o;
    o << "SMART CLEAN - removed:\n";
    for (int i = 0; i < k; i++) {
        Item r;
        if (items.smartCleanOnce(&r)) {
            undoStack.push(1, r);
            o << "  #" << r.id << " " << r.name << " (value " << PRIORITY_NAMES[r.priority] << ")\n";
        }
    }
    o << "\nUse Undo to bring items back one by one.";
    setOut(o.str());
    changed();
}

void onStats() {
    int st[3], cat[5];
    calcStats(st, cat);

    ostringstream o;
    o << "STATISTICS\n----------\n";

    int total = sumOf(3, st[LOST], st[FOUND], st[CLAIMED]);              // Exp 2
    o << "Total items            : " << total << "\n";
    o << "Lost / Found / Claimed : " << st[LOST] << " / " << st[FOUND] << " / " << st[CLAIMED] << "\n";

    int a = st[LOST], b = st[FOUND];                                     // Exp 3
    SetValue(&a, &b);
    o << "Open cases (lost+found): " << a << "\n";
    o << "Gap lost vs found      : " << b << "\n\n";

    int mx = maxOf(5, cat[0], cat[1], cat[2], cat[3], cat[4]);           // Exp 2
    int mn = minOf(5, cat[0], cat[1], cat[2], cat[3], cat[4]);
    o << "Items per category:\n";
    for (int i = 0; i < 5; i++) {
        char buf[64];
        sprintf(buf, "  %-12s ", CATEGORIES[i]);
        o << buf << string(cat[i], '#') << " " << cat[i] << (cat[i] == mx && mx > 0 ? "  <- most" : "") << "\n";
    }
    o << "\nMost reported  : " << mx << " item(s)\n";
    o << "Least reported : " << mn << " item(s)\n";
    o << "Pending claims : " << claims.size << "\n";
    setOut(o.str());
}

void onSearch()  { filterText = getText(hSearch); refreshList(); }
void onShowAll() { filterText = ""; SetWindowTextA(hSearch, ""); refreshList(); }

void showSelected() {
    int id = selectedId();
    Node* n = (id < 0) ? NULL : items.find(id);
    if (n) setOut(n->data.details());
}

// ---------- build the window ----------
void createControls() {
    hUI        = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hBold      = CreateFontA(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hSmallB    = CreateFontA(-12, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hCardTitle = CreateFontA(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hTitleF    = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hBig       = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hMono      = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    hBrField   = CreateSolidBrush(C_FIELD);

    // ================= LEFT CARD : report an item =================
    addLbl(36, 196, 1, "Report an item");
    btn("I lost something",  36, 226, 152, 34, ID_TLOST);
    btn("I found something", 192, 226, 152, 34, ID_TFOUND);

    addLbl(36, 272, 0, "ITEM NAME");
    hName = field(36, 290, 308, ID_NAME, 0);

    addLbl(36, 330, 0, "CATEGORY");
    btn(CATEGORIES[0], 36,  348, 98,  28, ID_CAT0);
    btn(CATEGORIES[1], 140, 348, 98,  28, ID_CAT1);
    btn(CATEGORIES[2], 244, 348, 100, 28, ID_CAT2);
    btn(CATEGORIES[3], 36,  382, 98,  28, ID_CAT3);
    btn(CATEGORIES[4], 140, 382, 98,  28, ID_CAT4);

    addLbl(36, 420, 0, "VALUE OF ITEM");
    btn("Low",    36,  438, 100, 28, ID_PLOW);
    btn("Medium", 140, 438, 100, 28, ID_PMED);
    btn("High",   244, 438, 100, 28, ID_PHIGH);

    addLbl(36, 476, 0, "LOCATION");
    hLoc = field(36, 494, 308, ID_LOC, 0);

    addLbl(36, 534, 0, "DATE (DD-MM-YYYY)");
    addLbl(192, 534, 0, "CONTACT NUMBER");
    hDate    = field(36, 552, 152, ID_DATE, 0);
    hContact = field(192, 552, 152, ID_CONTACT, 0);
    SetWindowTextA(hDate, todayText().c_str());

    btn("+  Add Item", 36, 596, 308, 40, ID_ADD);

    // ================= RIGHT CARD : table =================
    hSearch = field(392, 198, 430, ID_SEARCH, 26);
    btn("Search",   830, 198, 88, 32, ID_BTNSEARCH);
    btn("Show All", 926, 198, 96, 32, ID_SHOWALL);

    hList = mk("LISTBOX", "", WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
               LIST_X, 272, 812, 216, ID_LIST);
    SendMessageA(hList, LB_SETITEMHEIGHT, 0, ROW_H);

    btn("Delete Selected",  392, 498, 197, 36, ID_DELETE);
    btn("Undo Last Action", 597, 498, 197, 36, ID_UNDO);
    btn("Smart Match",      802, 498, 197, 36, ID_MATCH);
    btn("Statistics",      1007, 498, 197, 36, ID_STATS);

    // ================= RIGHT CARD : activity + claims =================
    addLbl(392, 546, 1, "Activity");
    hOut = mk("EDIT", "", WS_VSCROLL | ES_MULTILINE | ES_READONLY, 400, 578, 464, 140, ID_OUT);
    SendMessageA(hOut, WM_SETFONT, (WPARAM)hMono, TRUE);

    addLbl(888, 546, 1, "Claims queue");
    addLbl(888, 574, 0, "CLAIMANT NAME");
    addLbl(1050, 574, 0, "CONTACT");
    hClaimant     = field(888, 590, 154, ID_CLAIMANT, 0);
    hClaimContact = field(1050, 590, 154, ID_CLAIMCONTACT, 0);
    btn("Submit Claim",        888, 628, 154, 34, ID_SUBMITCLAIM);
    btn("Process Next Claim", 1050, 628, 154, 34, ID_PROCESSCLAIM);
    btn("View Pending Claims", 888, 668, 316, 30, ID_VIEWCLAIMS);

    hK = field(888, 704, 70, ID_K, 0);
    SetWindowTextA(hK, "1");
    btn("Smart Clean (remove K)", 966, 704, 238, 30, ID_CLEAN);

    setOut("Welcome! Add an item on the left, or click a row in the table.");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            hWnd = hwnd;
            createControls();
            loadData();
            refreshList();
            SetTimer(hwnd, 1, 60, NULL);        // hover effect polling
            return 0;

        case WM_ERASEBKGND:
            return 1;                            // we paint everything ourselves

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            int W = rc.right, H = rc.bottom;
            HDC mem = CreateCompatibleDC(hdc);                 // double buffering = no flicker
            HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
            HGDIOBJ old = SelectObject(mem, bmp);
            paintAll(mem, W, H);
            BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* m = (MEASUREITEMSTRUCT*)lParam;
            if (m->CtlType == ODT_LISTBOX) { m->itemHeight = ROW_H; return TRUE; }
            return FALSE;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)lParam;
            if (d->CtlType == ODT_BUTTON)       drawButton(d);
            else if (d->CtlType == ODT_LISTBOX) drawRow(d);
            return TRUE;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, C_DARK);
            SetBkColor(dc, C_FIELD);
            return (LRESULT)hBrField;
        }

        case WM_TIMER: {
            POINT p; GetCursorPos(&p);
            HWND h = WindowFromPoint(p);
            int nid = 0;
            if (h && h != hwnd && GetParent(h) == hwnd) {
                int cid = GetDlgCtrlID(h);
                if (cid >= ID_TLOST && cid <= ID_BTN_LAST) nid = cid;
            }
            if (nid != hoverId) {
                int old = hoverId;
                hoverId = nid;
                if (old) redrawCtl(old);
                if (nid) redrawCtl(nid);
            }
            return 0;
        }

        case WM_SETCURSOR:
            if (hoverId != 0 && LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            int id = LOWORD(wParam), code = HIWORD(wParam);
            if (id == ID_LIST && code == LBN_SELCHANGE) { showSelected(); return 0; }
            if (code != BN_CLICKED) return 0;

            if (id == ID_TLOST || id == ID_TFOUND) { curType = (id == ID_TFOUND) ? 1 : 0; redrawGroup(ID_TLOST, ID_TFOUND); return 0; }
            if (id >= ID_CAT0 && id <= ID_CAT4)    { curCat = id - ID_CAT0;               redrawGroup(ID_CAT0, ID_CAT4);   return 0; }
            if (id >= ID_PLOW && id <= ID_PHIGH)   { curPri = id - ID_PLOW + 1;           redrawGroup(ID_PLOW, ID_PHIGH);  return 0; }

            switch (id) {
                case ID_ADD:          onAdd();          break;
                case ID_DELETE:       onDelete();       break;
                case ID_UNDO:         onUndo();         break;
                case ID_MATCH:        onMatch();        break;
                case ID_STATS:        onStats();        break;
                case ID_SUBMITCLAIM:  onSubmitClaim();  break;
                case ID_PROCESSCLAIM: onProcessClaim(); break;
                case ID_VIEWCLAIMS:   onViewClaims();   break;
                case ID_CLEAN:        onClean();        break;
                case ID_BTNSEARCH:    onSearch();       break;
                case ID_SHOWALL:      onShowAll();      break;
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            saveData();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    FreeConsole();   // hides the black console window if compiled as a console app

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "DigitalLostFoundV2";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT r = {0, 0, 1240, 760};
    AdjustWindowRect(&r, style, FALSE);

    HWND w = CreateWindowA("DigitalLostFoundV2", "Digital Lost & Found", style,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, hInst, NULL);
    ShowWindow(w, nShow);
    UpdateWindow(w);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        if (!IsDialogMessage(w, &m)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    return (int)m.wParam;
}
