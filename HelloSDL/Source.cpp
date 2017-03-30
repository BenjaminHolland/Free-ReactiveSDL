#include <iostream>
#include <exception>
#include <map>
#include "SDL.h"
#include "rx.hpp"
#include "rx-subjects.hpp"
#include "rx-observer.hpp"
#include <functional>
#include <chrono>
#include <list>
#include <array>

namespace rx {
    using namespace rxcpp;
    using namespace rxcpp::sources;
}
//Screen Dimensions
const int SCREEN_WIDTH = 300;
const int SCREEN_HEIGHT = 300;


class Program {
private:
    
    //members for tracking SDL resources.
    SDL_Window* window;
    SDL_Renderer* renderer;

    //members for managing the event stream.
    bool continueEventStream = true;
    rx::connectable_observable<SDL_Event> eventStream;
    std::vector<rx::composite_subscription> subs;

    //members for managing the trail 
    static const size_t TRAIL_SIZE = 100;
    std::array<SDL_Point, TRAIL_SIZE> trail;

    //Initialize SDL resources.
    void initSdlResources() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::exception(SDL_GetError());
        }
        if (SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WindowFlags::SDL_WINDOW_SHOWN, &window, &renderer) < 0) {
            throw std::exception(SDL_GetError());
        }
    }
    
    //Pumps events through the event pipeleine untill the queue is empty, then renders the frame.
    //Stops when instructed to via setting continueEventStream to false.
    void eventStreamOnSubscribe(rx::subscriber<SDL_Event> head) {
        static SDL_Event e;
        while (this->continueEventStream) {
            while (SDL_PollEvent(&e)!=0) {
                head.on_next(e);
            }
            this->onRender();
        }
    }

    //Copies the data from the given trail buffer into the current trail buffer.
    void storeTrail(std::vector<SDL_Point> trail) {
        for (int i = 0; i < trail.size(); i++) {
            this->trail[i] = trail.at(i);
        }
    }

    //Sets up the handler for the quit event.
    void initQuitHandler() {
        auto sub = eventStream
            .filter([](SDL_Event e) {return e.type == SDL_QUIT; })
            .subscribe([&](SDL_Event e) {this->continueEventStream = false; });
        subs.push_back(std::move(sub));
    }

    //Sets up the handler for the mouse motion event.
    void initMouseMotionHandler() {
        auto sub = eventStream
            .filter([](SDL_Event& e) {return e.type == SDL_MOUSEMOTION; })
            .map([](SDL_Event e) {SDL_Point r = { e.motion.x,e.motion.y }; return r; })
            .buffer(TRAIL_SIZE, 1)
            .subscribe([&](std::vector<SDL_Point> trail) {this->storeTrail(trail); });
        subs.push_back(std::move(sub));
    }

    //Create the event stream and prepare it for subscriptions.
    void initEventStream() {
        eventStream = rx::sources::create<SDL_Event>([&](rx::subscriber<SDL_Event> subscriber) {
            this->eventStreamOnSubscribe(subscriber);
        }).publish();
    }
   

    //Render the trail.
    void onRender() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLines(renderer, trail.data(), trail.size());
        SDL_RenderPresent(renderer);
    }

    //Clean up the SDL resources we're using. 
    void freeSdlResources() {
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        if (renderer != nullptr){
            SDL_DestroyRenderer(renderer);
        }
    }

public:
    Program() {
      
        initSdlResources();
        initEventStream();
        initMouseMotionHandler();
        initQuitHandler();
    }
    void run() {
        eventStream.connect();
    }
    ~Program() {
        freeSdlResources();
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {

    Program instance;
    instance.run();
    return 0;

}