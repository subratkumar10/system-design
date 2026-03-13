#include <iostream>
#include <string>

// 1) Define the State interface first so it's a complete type for calls and delete.
class MediaPlayer; // forward declare context for method signatures

class PlayerState {
public:
    virtual ~PlayerState() {}
    virtual void play(MediaPlayer* ctx)  = 0;
    virtual void pause(MediaPlayer* ctx) = 0;
    virtual void stop(MediaPlayer* ctx)  = 0;
    virtual const char* name() const     = 0;
};

// 2) Define the Context next. It stores pointers to complete PlayerState.
class MediaPlayer {
public:
    MediaPlayer()
        : state_(nullptr), stopped_(nullptr), playing_(nullptr), paused_(nullptr) {}

    ~MediaPlayer() {
        delete stopped_;
        delete playing_;
        delete paused_;
    }

    // Public API
    void play()  { if (state_) state_->play(this); }
    void pause() { if (state_) state_->pause(this); }
    void stop()  { if (state_) state_->stop(this); }

    // Data helpers
    void loadTrack(const std::string& t) { track_ = t; }
    const std::string& track() const { return track_; }

    // Initialize state singletons (owned by this context)
    void initStates(PlayerState* stopped, PlayerState* playing, PlayerState* paused) {
        stopped_ = stopped;
        playing_ = playing;
        paused_  = paused;
        setState(stopped_);
    }

    // State management
    void setState(PlayerState* s) {
        state_ = s;
        std::cout << "State -> " << state_->name() << "\n";
    }
    PlayerState* stopped() { return stopped_; }
    PlayerState* playing() { return playing_; }
    PlayerState* paused()  { return paused_; }

private:
    std::string   track_;
    PlayerState*  state_;
    PlayerState*  stopped_;
    PlayerState*  playing_;
    PlayerState*  paused_;
};

// 3) Concrete states with inline method definitions.
// They can call MediaPlayer methods (MediaPlayer is fully declared above).

class StoppedState : public PlayerState {
public:
    void play(MediaPlayer* ctx) override {
        if (ctx->track().empty()) {
            std::cout << "No track loaded. Loading default...\n";
        }
        std::cout << "Starting playback.\n";
        ctx->setState(ctx->playing());
    }
    void pause(MediaPlayer*) override {
        std::cout << "Cannot pause: already stopped.\n";
    }
    void stop(MediaPlayer*) override {
        std::cout << "Already stopped.\n";
    }
    const char* name() const override { return "Stopped"; }
};

class PlayingState : public PlayerState {
public:
    void play(MediaPlayer*) override {
        std::cout << "Already playing.\n";
    }
    void pause(MediaPlayer* ctx) override {
        std::cout << "Pausing playback.\n";
        ctx->setState(ctx->paused());
    }
    void stop(MediaPlayer* ctx) override {
        std::cout << "Stopping playback.\n";
        ctx->setState(ctx->stopped());
    }
    const char* name() const override { return "Playing"; }
};

class PausedState : public PlayerState {
public:
    void play(MediaPlayer* ctx) override {
        std::cout << "Resuming playback.\n";
        ctx->setState(ctx->playing());
    }
    void pause(MediaPlayer*) override {
        std::cout << "Already paused.\n";
    }
    void stop(MediaPlayer* ctx) override {
        std::cout << "Stopping from paused.\n";
        ctx->setState(ctx->stopped());
    }
    const char* name() const override { return "Paused"; }
};

// Demo
int main() {
    MediaPlayer player;

    // Allocate states (owned by player)
    PlayerState* stopped = new StoppedState();
    PlayerState* playing = new PlayingState();
    PlayerState* paused  = new PausedState();
    player.initStates(stopped, playing, paused);

    player.loadTrack("song.mp3");

    player.play();   // Stopped -> Playing
    player.pause();  // Playing -> Paused
    player.play();   // Paused -> Playing
    player.stop();   // Playing -> Stopped
    player.pause();  // Invalid in Stopped

    return 0;
}