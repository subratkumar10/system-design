// C++17 single-file: examples of common design patterns with small demos.
// Build: g++ -std=c++17 patterns.cpp -o patterns && ./patterns

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/*
Where patterns fit in a movie booking system 

    Creational 
        Factory/Abstract Factory: create payment providers (Stripe, PayPal), ticket types (adult, child, VIP), notification senders (email, SMS).
        Builder: assemble a Booking with selected showtime, seats, pricing, and applied promotions.
        Prototype: clone seat maps or pricing templates per auditorium/show.
         

    Structural 
        Facade: a BookingService that exposes simple APIs (search, lock seats, pay, issue ticket) hiding complex subsystems (inventory, payments, notifications).
        Adapter: unify multiple payment SDKs, SMS/email APIs, or third-party loyalty systems.
        Bridge: separate UI actions (web/mobile) from underlying operations; or separate “content” (movie/show) from “presentation” (region-specific formats).
        Composite: represent seat maps (sections/rows/seats) or bundles (ticket + snack combo).
        Decorator: add features to a ticket (3D glasses, lounge access) or add cross-cutting behavior (logging, rate limiting) around services.
        Proxy: caching show listings, lazy-loading movie details, or access-control to admin APIs.
        Flyweight: share intrinsic seat data (seat type, price tier) across many seats to save memory.
         

    Behavioral 
        Strategy: pricing strategies (peak/off-peak, loyalty discounts), seat ranking, payment methods.
        State: booking lifecycle (Selecting -> Reserved -> Paid -> Cancelled/Expired).
        Observer: notify users on status changes (reservation expiry, payment success), notify inventory for updates.
        Command: encapsulate actions (ReserveSeat, Pay, Cancel) for audit/undo/retry.
        Chain of Responsibility: request validation (age rating, seating constraints, payment checks) or promotion eligibility.
        Mediator: coordinate UI widgets (date picker, auditorium, seat grid).
        Template Method: checkout flow with overridable hooks per region/brand.
        Memento: restore a cart/session after refresh or reconnection.
*/

// 1) Singleton (Meyers’ Singleton)
class Logger {
public:
  static Logger& instance() { static Logger inst; return inst; }
  void log(const std::string& msg) { std::cout << "[LOG] " << msg << "\n"; }
private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};
void demo_singleton() {
  std::cout << "\n== Singleton ==\n";
  Logger::instance().log("Singleton works");
}

// 2) Factory Method
struct Product { virtual ~Product() = default; virtual std::string name() const = 0; };
struct ProductA : Product { std::string name() const override { return "A"; } };
struct ProductB : Product { std::string name() const override { return "B"; } };
struct Creator { virtual ~Creator() = default; virtual std::unique_ptr<Product> make() const = 0; };
struct CreatorA : Creator { std::unique_ptr<Product> make() const override { return std::make_unique<ProductA>(); } };
struct CreatorB : Creator { std::unique_ptr<Product> make() const override { return std::make_unique<ProductB>(); } };
void demo_factory_method() {
  std::cout << "\n== Factory Method ==\n";
  std::unique_ptr<Creator> a = std::make_unique<CreatorA>(), b = std::make_unique<CreatorB>();
  std::cout << "Created: " << a->make()->name() << " and " << b->make()->name() << "\n";
}

// 3) Builder
struct HttpRequest { std::string method, host, path; 
    std::string str() const { return method + " " + host + path; } };
class HttpRequestBuilder {
  HttpRequest r_;
public:
  HttpRequestBuilder& method(std::string m){ r_.method = std::move(m); return *this; }
  HttpRequestBuilder& host(std::string h){ r_.host = std::move(h); return *this; }
  HttpRequestBuilder& path(std::string p){ r_.path = std::move(p); return *this; }
  HttpRequest build(){ return r_; }
};
void demo_builder() {
  std::cout << "\n== Builder ==\n";
  auto r = HttpRequestBuilder{}.method("GET").host("example.com").path("/index.html").build();
  std::cout << r.str() << "\n";
}

// 4) Strategy
class SortStrategy { public: virtual ~SortStrategy() = default; virtual void sortVec(std::vector<int>& v) const = 0; };
class Asc : public SortStrategy { void sortVec(std::vector<int>& v) const override { std::sort(v.begin(), v.end()); } };
class Desc : public SortStrategy { void sortVec(std::vector<int>& v) const override { std::sort(v.begin(), v.end(), std::greater<>()); } };
class Sorter {
  std::unique_ptr<SortStrategy> s_;
public:
  explicit Sorter(std::unique_ptr<SortStrategy> s): s_(std::move(s)) {}
  void run(std::vector<int>& v) const { s_->sortVec(v); }
};
void demo_strategy() {
  std::cout << "\n== Strategy ==\n";
  std::vector<int> v{3,1,4,1,5};
  Sorter a(std::make_unique<Asc>()); a.run(v);
  std::cout << "Asc: "; for (auto x: v) std::cout << x << " "; std::cout << "\n";
  Sorter d(std::make_unique<Desc>()); d.run(v);
  std::cout << "Desc: "; for (auto x: v) std::cout << x << " "; std::cout << "\n";
}

// 5) Observer
class Subject {
  std::vector<std::function<void(int)>> subs_;
public:
  void subscribe(std::function<void(int)> cb){ subs_.push_back(std::move(cb)); }
  void set_state(int s){ for (auto& cb : subs_) cb(s); }
};
void demo_observer() {
  std::cout << "\n== Observer ==\n";
  Subject s;
  s.subscribe([](int v){ std::cout << "Observer1: " << v << "\n"; });
  s.subscribe([](int v){ std::cout << "Observer2: " << v*2 << "\n"; });
  s.set_state(21);
}

// 6) Decorator
struct Stream { virtual ~Stream()=default; virtual void write(std::string_view s)=0; };
struct ConsoleStream : Stream { void write(std::string_view s) override { std::cout << s; } };
struct StreamDecorator : Stream {
  std::unique_ptr<Stream> inner;
  explicit StreamDecorator(std::unique_ptr<Stream> s): inner(std::move(s)) {}
  void write(std::string_view s) override { inner->write(s); }
};
struct UppercaseStream : StreamDecorator {
  using StreamDecorator::StreamDecorator;
  void write(std::string_view s) override {
    std::string up(s);
    for (auto& c: up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    inner->write(up);
  }
};
void demo_decorator() {
  std::cout << "\n== Decorator ==\n";
  std::unique_ptr<Stream> s = std::make_unique<UppercaseStream>(std::make_unique<ConsoleStream>());
  s->write("hello world\n");
}

// 7) Command
struct Command { virtual ~Command()=default; virtual void execute()=0; };
struct Light { bool on=false; void turnOn(){ on=true; std::cout<<"Light ON\n"; } void turnOff(){ on=false; std::cout<<"Light OFF\n"; } };
struct TurnOn : Command { Light& l; explicit TurnOn(Light& li): l(li){} void execute() override { l.turnOn(); } };
struct TurnOff : Command { Light& l; explicit TurnOff(Light& li): l(li){} void execute() override { l.turnOff(); } };
struct Remote {
  std::vector<std::unique_ptr<Command>> q;
  void add(std::unique_ptr<Command> c){ q.push_back(std::move(c)); }
  void run(){ for (auto& c: q) c->execute(); q.clear(); }
};
void demo_command() {
  std::cout << "\n== Command ==\n";
  Light light; Remote r;
  r.add(std::make_unique<TurnOn>(light));
  r.add(std::make_unique<TurnOff>(light));
  r.run();
}

// 8) Adapter
struct OldAPI { void do_work(int x){ std::cout << "OldAPI: " << x << "\n"; } };
struct NewAPI { virtual ~NewAPI()=default; virtual void run(std::string s)=0; };
struct Adapter : NewAPI {
  OldAPI& old; explicit Adapter(OldAPI& o): old(o) {}
  void run(std::string s) override { old.do_work(std::stoi(s)); }
};
void demo_adapter() {
  std::cout << "\n== Adapter ==\n";
  OldAPI legacy; Adapter a(legacy);
  a.run("42");
}

// 9) Bridge
struct Renderer { virtual ~Renderer()=default; virtual void drawCircle(float x,float y,float r)=0; };
struct VectorRenderer : Renderer { void drawCircle(float x,float y,float r) override { std::cout<<"Vector circle at "<<x<<","<<y<<" r="<<r<<"\n"; } };
struct RasterRenderer : Renderer { void drawCircle(float x,float y,float r) override { std::cout<<"Raster circle at "<<x<<","<<y<<" r="<<r<<"\n"; } };
struct Shape { Renderer& rnd; explicit Shape(Renderer& r): rnd(r){} virtual ~Shape()=default; virtual void draw()=0; };
struct Circle : Shape { float x,y,r; Circle(Renderer& r,float X,float Y,float R): Shape(r),x(X),y(Y),r(R){} void draw() override { rnd.drawCircle(x,y,r); } };
void demo_bridge() {
  std::cout << "\n== Bridge ==\n";
  VectorRenderer vr; RasterRenderer rr;
  Circle c1(vr,0,0,5), c2(rr,1,1,3);
  c1.draw(); c2.draw();
}

// 10) Composite
struct Node {
  virtual ~Node()=default;
  virtual int value() const=0;
  virtual void add(std::shared_ptr<Node>) { /* leaf: no-op */ }
};
struct Leaf : Node { int v; explicit Leaf(int x): v(x){} int value() const override { return v; } };
struct CompositeNode : Node {
  std::vector<std::shared_ptr<Node>> children;
  void add(std::shared_ptr<Node> n) override { children.push_back(std::move(n)); }
  int value() const override { int s=0; for (auto& c: children) s += c->value(); return s; }
};
void demo_composite() {
  std::cout << "\n== Composite ==\n";
  auto root = std::make_shared<CompositeNode>();
  root->add(std::make_shared<Leaf>(1));
  auto sub = std::make_shared<CompositeNode>(); sub->add(std::make_shared<Leaf>(2)); sub->add(std::make_shared<Leaf>(3));
  root->add(sub);
  std::cout << "Sum: " << root->value() << "\n";
}

// 11) State
struct State { virtual ~State()=default; virtual void handle(bool& isOpen)=0; };
struct OpenState : State { void handle(bool& isOpen) override { std::cout<<"Already open\n"; isOpen=true; } };
struct ClosedState : State { void handle(bool& isOpen) override { std::cout<<"Opening\n"; isOpen=true; } };
struct Door {
  bool isOpen=false;
  std::unique_ptr<State> state = std::make_unique<ClosedState>();
  void open(){ if(!isOpen) state = std::make_unique<OpenState>(); state->handle(isOpen); }
  void close(){ if(isOpen){ std::cout<<"Closing\n"; isOpen=false; state = std::make_unique<ClosedState>(); } else std::cout<<"Already closed\n"; }
};
void demo_state() {
  std::cout << "\n== State ==\n";
  Door d; d.open(); d.open(); d.close(); d.close();
}

// 12) Visitor
struct CircleV; struct RectV;
struct VVisitor { virtual ~VVisitor()=default; virtual void visit(CircleV&)=0; virtual void visit(RectV&)=0; };
struct ShapeV { virtual ~ShapeV()=default; virtual void accept(VVisitor& v)=0; };
struct CircleV : ShapeV { float r{1}; void accept(VVisitor& v) override { v.visit(*this); } };
struct RectV : ShapeV { float w{2}, h{3}; void accept(VVisitor& v) override { v.visit(*this); } };
void demo_visitor() {
  std::cout << "\n== Visitor ==\n";
  struct AreaVisitor : VVisitor {
    double area=0;
    void visit(CircleV& c) override { static constexpr double PI = 3.141592653589793; area += PI * c.r * c.r; }
    void visit(RectV& r) override { area += r.w * r.h; }
  } vis;
  std::vector<std::unique_ptr<ShapeV>> shapes;
  shapes.push_back(std::make_unique<CircleV>());
  shapes.push_back(std::make_unique<RectV>());
  for (auto& s: shapes) s->accept(vis);
  std::cout << "Total area: " << vis.area << "\n";
}

// 13) Prototype
struct Proto {
  virtual ~Proto()=default;
  virtual std::unique_ptr<Proto> clone() const=0;
  virtual void show() const=0;
};
struct ConcreteProto : Proto {
  int x{0}; std::string tag{"p"};
  ConcreteProto(int x_=0,std::string t="p"): x(x_), tag(std::move(t)){}
  std::unique_ptr<Proto> clone() const override { return std::make_unique<ConcreteProto>(*this); }
  void show() const override { std::cout << "Prototype: " << tag << " x=" << x << "\n"; }
};
void demo_prototype() {
  std::cout << "\n== Prototype ==\n";
  ConcreteProto p1(7,"orig");
  auto p2 = p1.clone(); static_cast<ConcreteProto&>(*p2).x = 8;
  p1.show(); p2->show();
}

// 14) Facade
struct CPU { void run(){ std::cout<<"CPU run\n"; } };
struct Memory { void load(){ std::cout<<"Memory load\n"; } };
struct Disk { void read(){ std::cout<<"Disk read\n"; } };
struct Computer {
  CPU cpu; Memory mem; Disk disk;
  void start(){ disk.read(); mem.load(); cpu.run(); }
};
void demo_facade() {
  std::cout << "\n== Facade ==\n";
  Computer c; c.start();
}

int main() {
  demo_singleton();
  demo_factory_method();
  demo_builder();
  demo_strategy();
  demo_observer();
  demo_decorator();
  demo_command();
  demo_adapter();
  demo_bridge();
  demo_composite();
  demo_state();
  demo_visitor();
  demo_prototype();
  demo_facade();
  return 0;
}