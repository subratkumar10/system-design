#include <iostream>
using namespace std;

// --- Subsystems ---
class CPU {
public:
    void freeze() { cout << "CPU frozen\n"; }
    void jump(long position) { cout << "CPU jumping to " << position << "\n"; }
    void execute() { cout << "CPU executing\n"; }
};

class Memory {
public:
    void load(long position, string data) {
        cout << "Loading data '" << data << "' into memory at " << position << "\n";
    }
};

class HardDrive {
public:
    string read(long lba, int size) {
        return "boot_sector_data";
    }
};

// --- Facade ---
class ComputerFacade {
private:
    CPU cpu;
    Memory memory;
    HardDrive hardDrive;

public:
    void startComputer() {
        cout << "Starting computer...\n";
        cpu.freeze();
        string bootData = hardDrive.read(0, 1024);
        memory.load(0, bootData);
        cpu.jump(0);
        cpu.execute();
        cout << "Computer started successfully!\n";
    }
};

int main(){

    ComputerFacade computerFacade ;
    computerFacade.startComputer();
}