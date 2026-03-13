// simple_word_processor_cpp14.cpp
// Build: clang++ -std=c++14 -Wall -Wextra -pedantic simple_word_processor_cpp14.cpp -c

#include <string>
#include <vector>

// Basic aliases
typedef int NodeId;
typedef int StyleId;

// ================== doc: Attributes (Flyweight for styles) ==================
namespace doc {

struct Color {
    double r, g, b, a;
    Color(): r(0), g(0), b(0), a(1) {}
};

struct CharAttr {
    std::string fontFamily;
    double fontSize;
    int bold; int italic; int underline; // 0/1
    Color color;
    std::string lang;
    CharAttr(): fontFamily("Segoe UI"), fontSize(11.0),
                bold(0), italic(0), underline(0), color(), lang("en-US") {}
};

struct ParagraphAttr {
    int alignment; // 0=Left,1=Right,2=Center,3=Justify
    double spacingBefore, spacingAfter, lineSpacing;
    double indentLeft, indentRight, indentFirstLine;
    ParagraphAttr(): alignment(0), spacingBefore(0.0), spacingAfter(6.0), lineSpacing(1.15),
                     indentLeft(0.0), indentRight(0.0), indentFirstLine(0.0) {}
};

struct TableAttr {
    double borderWidth;
    TableAttr(): borderWidth(0.5) {}
};

// ================== doc: Text storage (very simple placeholder) =============
class PieceTable {
public:
    std::u32string text; // super simple: just a buffer (replace with real piece table)
    void insert(int pos, const std::u32string& s) {
        if (pos < 0) pos = 0;
        if (pos > (int)text.size()) pos = (int)text.size();
        text.insert(text.begin()+pos, s.begin(), s.end());
    }
    void erase(int pos, int len) {
        if (pos < 0 || len <= 0) return;
        if (pos >= (int)text.size()) return;
        int end = pos + len;
        if (end > (int)text.size()) end = (int)text.size();
        text.erase(text.begin()+pos, text.begin()+end);
    }
    int length() const { return (int)text.size(); }
    std::u32string substr(int pos, int len) const {
        if (pos < 0) pos = 0;
        if (pos >= (int)text.size()) return U"";
        int end = pos + len;
        if (end > (int)text.size()) end = (int)text.size();
        return std::u32string(text.begin()+pos, text.begin()+end);
    }
};

// ================== doc: Model (Composite: Document->Sections->Blocks->Inlines)
class InlineObj {
public:
    NodeId id;
    int kind; // 0=TextRun,1=Field,2=InlineImage
    InlineObj(): id(0), kind(0) {}
    virtual ~InlineObj() {}
};

class TextRun : public InlineObj {
public:
    int start;   // position in story text
    int length;  // length
    CharAttr attr;
    TextRun(): start(0), length(0) { kind = 0; }
};

class FieldInline : public InlineObj {
public:
    std::string fieldType;
    FieldInline() { kind = 1; }
};

class InlineImage : public InlineObj {
public:
    std::string imageId;
    double width, height;
    InlineImage(): width(0), height(0) { kind = 2; }
};

class Block {
public:
    NodeId id;
    int type; // 0=Paragraph,1=Table
    Block(): id(0), type(0) {}
    virtual ~Block() {}
};

class Paragraph : public Block {
public:
    ParagraphAttr parAttr;
    std::vector<InlineObj*> inlines; // owns InlineObj*
    Paragraph() { type = 0; }
    ~Paragraph() {
        for (size_t i=0;i<inlines.size();++i) delete inlines[i];
        inlines.clear();
    }
};

class TableCell {
public:
    std::vector<Block*> blocks; // owns Block*
    ~TableCell() {
        for (size_t i=0;i<blocks.size();++i) delete blocks[i];
        blocks.clear();
    }
};

class TableRow {
public:
    std::vector<TableCell> cells;
};

class Table : public Block {
public:
    TableAttr attr;
    std::vector<TableRow> rows;
    Table() { type = 1; }
};

class HeaderFooter {
public:
    std::vector<Block*> blocks; // owns Block*
    ~HeaderFooter() {
        for (size_t i=0;i<blocks.size();++i) delete blocks[i];
        blocks.clear();
    }
};

class Section {
public:
    double pageWidth, pageHeight;
    double marginLeft, marginRight, marginTop, marginBottom;
    int columns;
    HeaderFooter header, footer;
    std::vector<Block*> blocks; // owns Block*

    Section(): pageWidth(612.0), pageHeight(792.0),
               marginLeft(72.0), marginRight(72.0),
               marginTop(72.0), marginBottom(72.0),
               columns(1) {}
    ~Section() {
        for (size_t i=0;i<blocks.size();++i) delete blocks[i];
        blocks.clear();
    }
};

class Style {
public:
    StyleId id;
    std::string name;
    StyleId basedOn; // -1 if none
    int hasCharAttr; CharAttr charAttr;
    int hasParAttr;  ParagraphAttr parAttr;
    Style(): id(0), basedOn(-1), hasCharAttr(0), hasParAttr(0) {}
};

// Flyweight repo: store shared styles
class StyleSheet {
public:
    std::vector<Style> styles;
    void add(const Style& s) { styles.push_back(s); }
    int find(StyleId sid) const { for (int i=0;i<(int)styles.size();++i) if (styles[i].id==sid) return i; return -1; }
};

class Story {
public:
    NodeId id;
    PieceTable buffer;
    Story(): id(0) {}
};

class DocumentSettings {
public:
    std::string defaultLang;
    std::string defaultFont;
    int trackChanges; // 0/1
    DocumentSettings(): defaultLang("en-US"), defaultFont("Segoe UI"), trackChanges(0) {}
};

class Document {
public:
    std::vector<Section> sections;
    StyleSheet styles;
    DocumentSettings settings;
    Story mainStory;

    Paragraph* createParagraph(Section& s) { Paragraph* p = new Paragraph(); s.blocks.push_back(p); return p; }
    Table* createTable(Section& s) { Table* t = new Table(); s.blocks.push_back(t); return t; }
};

struct Pos { int section, block, inlineIndex, offset; Pos(): section(0), block(0), inlineIndex(0), offset(0) {} };
struct Range { Pos start, end; };

} // namespace doc

// ================== edit: Command (with simple Memento) =====================
// Pattern: Command; Memento via simple backups for undo
namespace edit {

class Context {
public:
    doc::Document* document;
    Context(): document(0) {}
};

class Command {
public:
    virtual ~Command() {}
    virtual void execute(Context& ctx) = 0;
    virtual void undo(Context& ctx) = 0;
};

class InsertTextCommand : public Command {
public:
    int pos; // where in story buffer (for simplicity: linear)
    std::u32string text;
    InsertTextCommand(): pos(0) {}

    void execute(Context& ctx) override {
        if (!ctx.document) return;
        ctx.document->mainStory.buffer.insert(pos, text);
    }
    void undo(Context& ctx) override {
        if (!ctx.document) return;
        ctx.document->mainStory.buffer.erase(pos, (int)text.size());
    }
};

class DeleteRangeCommand : public Command {
public:
    int pos;
    int len;
    std::u32string backup; // memento
    DeleteRangeCommand(): pos(0), len(0) {}
    void execute(Context& ctx) override {
        if (!ctx.document) return;
        backup = ctx.document->mainStory.buffer.substr(pos, len);
        ctx.document->mainStory.buffer.erase(pos, len);
    }
    void undo(Context& ctx) override {
        if (!ctx.document) return;
        ctx.document->mainStory.buffer.insert(pos, backup);
    }
};

// CommandManager without smart pointers
class CommandManager {
public:
    std::vector<Command*> undoStack;
    std::vector<Command*> redoStack;

    ~CommandManager() {
        clear(undoStack);
        clear(redoStack);
    }

    void apply(Context& ctx, Command* cmd) {
        if (!cmd) return;
        cmd->execute(ctx);
        clear(redoStack);
        undoStack.push_back(cmd);
    }
    void undo(Context& ctx) {
        int n=(int)undoStack.size();
        if (n==0) return;
        Command* c=undoStack.back(); undoStack.pop_back();
        c->undo(ctx);
        redoStack.push_back(c);
    }
    void redo(Context& ctx) {
        int n=(int)redoStack.size();
        if (n==0) return;
        Command* c=redoStack.back(); redoStack.pop_back();
        c->execute(ctx);
        undoStack.push_back(c);
    }

private:
    void clear(std::vector<Command*>& s) { for (size_t i=0;i<s.size();++i) delete s[i]; s.clear(); }
};

} // namespace edit

// ================== layout: Strategy (pluggable) ===========================
// Pattern: Strategy for shaping/line-breaking/pagination
namespace layout {

struct LineBox { int start; int len; LineBox(): start(0), len(0) {} };
struct ParagraphLayout { std::vector<LineBox> lines; };
struct PageFrame { std::vector<ParagraphLayout> paragraphs; };

class Shaper {
public:
    virtual ~Shaper() {}
    virtual void shape(const std::u32string& text) = 0;
};

class SimpleShaper : public Shaper {
public:
    void shape(const std::u32string& text) override { (void)text; /* naive */ }
};

class LineBreaker {
public:
    virtual ~LineBreaker() {}
    virtual void breakLines(const std::u32string& text, double maxWidth, std::vector<LineBox>& out) = 0;
};

class GreedyLineBreaker : public LineBreaker {
public:
    void breakLines(const std::u32string& text, double maxWidth, std::vector<LineBox>& out) override {
        (void)maxWidth;
        // naive: single line
        LineBox lb; lb.start=0; lb.len=(int)text.size();
        out.push_back(lb);
    }
};

class Paginator {
public:
    virtual ~Paginator() {}
    virtual std::vector<PageFrame> paginate(const std::vector<ParagraphLayout>& paras, double pageW, double pageH) = 0;
};

class SimplePaginator : public Paginator {
public:
    std::vector<PageFrame> paginate(const std::vector<ParagraphLayout>& paras, double pageW, double pageH) override {
        (void)pageW; (void)pageH;
        PageFrame p; p.paragraphs = paras; return std::vector<PageFrame>(1, p);
    }
};

class LayoutEngine {
public:
    Shaper* shaper;
    LineBreaker* breaker;
    Paginator* paginator;

    LayoutEngine(): shaper(0), breaker(0), paginator(0) {}

    std::vector<PageFrame> layoutDocument(const doc::Document& d) {
        std::vector<ParagraphLayout> paras;
        // naive: treat entire story as one paragraph
        std::u32string all = d.mainStory.buffer.text;
        if (breaker) {
            ParagraphLayout pl;
            breaker->breakLines(all, 500.0, pl.lines);
            paras.push_back(pl);
        }
        if (paginator) return paginator->paginate(paras, 612.0, 792.0);
        return std::vector<PageFrame>();
    }
};

} // namespace layout

// ================== render: Adapter/Facade =================================
// Pattern: Adapter (Canvas abstraction), Facade (Renderer)
namespace render {

class Canvas {
public:
    virtual ~Canvas() {}
    virtual void drawText(int x, int y, const std::string& utf8) = 0;
    virtual void drawRect(int x, int y, int w, int h) = 0;
};

class Renderer {
public:
    void renderPages(const std::vector<layout::PageFrame>& pages, Canvas& canvas) {
        (void)pages; (void)canvas;
        // draw based on layout (omitted)
    }
};

} // namespace render

// ================== controller: Facade over subsystems =====================
// Pattern: Facade (simple API over commands + layout)
namespace controller {

class Editor {
public:
    doc::Document* document;
    edit::CommandManager* cmdMgr;
    layout::LayoutEngine* layoutEngine;

    Editor(): document(0), cmdMgr(0), layoutEngine(0) {}

    void insertAt(int pos, const std::u32string& text) {
        if (!document || !cmdMgr) return;
        edit::Context ctx; ctx.document = document;
        edit::InsertTextCommand* c = new edit::InsertTextCommand();
        c->pos = pos; c->text = text;
        cmdMgr->apply(ctx, c);
        relayout();
    }

    void deleteAt(int pos, int len) {
        if (!document || !cmdMgr) return;
        edit::Context ctx; ctx.document = document;
        edit::DeleteRangeCommand* c = new edit::DeleteRangeCommand();
        c->pos = pos; c->len = len;
        cmdMgr->apply(ctx, c);
        relayout();
    }

    void undo() {
        if (!document || !cmdMgr) return;
        edit::Context ctx; ctx.document = document;
        cmdMgr->undo(ctx);
        relayout();
    }

    void redo() {
        if (!document || !cmdMgr) return;
        edit::Context ctx; ctx.document = document;
        cmdMgr->redo(ctx);
        relayout();
    }

    std::vector<layout::PageFrame> pages;

private:
    void relayout() {
        if (!layoutEngine || !document) return;
        pages = layoutEngine->layoutDocument(*document);
    }
};

} // namespace controller

// ================== usage wiring (commented example) =======================
// Uncomment to test as an executable
/*
#include <iostream>
#include <codecvt>
#include <locale>

class ConsoleCanvas : public render::Canvas {
public:
    void drawText(int, int, const std::string& utf8) override { std::cout << utf8 << "\n"; }
    void drawRect(int, int, int, int) override {}
};

static std::string to_utf8(const std::u32string& s) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    return conv.to_bytes(s);
}

int main() {
    doc::Document d;
    d.sections.push_back(doc::Section());
    layout::SimpleShaper sh;
    layout::GreedyLineBreaker br;
    layout::SimplePaginator pg;
    layout::LayoutEngine engine; engine.shaper=&sh; engine.breaker=&br; engine.paginator=&pg;

    edit::CommandManager mgr;
    controller::Editor ed; ed.document=&d; ed.cmdMgr=&mgr; ed.layoutEngine=&engine;

    ed.insertAt(0, U"Hello ");
    ed.insertAt(6, U"World");
    ed.deleteAt(5, 1);
    ed.undo();
    ed.redo();

    ConsoleCanvas canvas;
    render::Renderer renderer;
    renderer.renderPages(ed.pages, canvas);
    std::cout << to_utf8(d.mainStory.buffer.text) << std::endl;
    return 0;
}
*/
