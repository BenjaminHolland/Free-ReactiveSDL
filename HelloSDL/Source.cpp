#include <iostream>
#include <exception>
#include <map>
#include "SDL.h"
#include "rx.hpp"
#include "rx-subjects.hpp"
#include "rx-observer.hpp"

#include <chrono>
#include <list>
#include <array>

//Screen Dimensions
const int SCREEN_WIDTH = 300;
const int SCREEN_HEIGHT = 300;

//How Long our trail is.
const int TRAIL_LENGTH = 100;
//returns an observable with this frames events in it. Polls untill there are no more events left in the queue. 
rxcpp::observable<SDL_Event> getFrameEvents() {

    return rxcpp::sources::create<SDL_Event>([](rxcpp::subscriber<SDL_Event> current) {
        //Drian all the events into the observer. 
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            current.on_next(e);
        };

        //Complete this observer.
        current.on_completed();
    });
}

//Get a constant stream of SDL events, while periodically calling the given render method after each "pause". Monitors the "quit" variable and completes when ordered to.
template<typename F>
rxcpp::observable<rxcpp::observable<SDL_Event>> getEventStream(volatile bool& quit, F& render) {
    return rxcpp::sources::create<rxcpp::observable<SDL_Event>>([&quit, &render](rxcpp::subscriber<rxcpp::observable<SDL_Event>> dispatcher) {

        //check wether we're supposed to quit. There's probably a better way to do this. 
        while (!quit) {

            //pump this frames events downstream to anyone who may be watching. 
            dispatcher.on_next(getFrameEvents());
            //We should probably do some sort of backpressure checking here. 

            //Render the frame.
            render();

        }

        //notify subscribers that the program is ending. 
        dispatcher.on_completed();
    });
}


int main(int argc, char* argv[]) {

    //Initialize SDL
    SDL_Window* windowMain = NULL;
    SDL_Surface* surfaceMain = NULL;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::exception("SDL did not initialize.");
    }

    windowMain = SDL_CreateWindow("Hello SDL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (windowMain == NULL) {
        throw std::exception("SDL Window did not initialize.");
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(windowMain, -1, SDL_RENDERER_ACCELERATED);

    //Set up our program quit switch. Again, there's probably a better way to do this.
    //Come to think of it, because all of this is now working synchronously, there's really no need for this to be volatile anymore. 
    volatile bool quit = false;

    //Set up our trail buffer. It's very simple. 
    std::array<SDL_Point, TRAIL_LENGTH> points;

    //Not used yet. 
    long long int frame = 0;


    //Set up our color components. Again, this should be changed to non-volatile, since everything we're doing here is synchronous.
    volatile Uint8 red = 255;
    volatile Uint8 green = 255;
    volatile Uint8 blue = 255;

    //Get the event stream. the concat and publish operators should probably be wrapped into the function.
    auto eventStream = getEventStream(quit, [&renderer, &points, &red, &green, &blue]() {

        //Clear the screen.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        //Draw the trail. 
        SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
        for (int i = 0; i < points.size() - 1; i++) {
            SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
        }

        //Flip!
        SDL_RenderPresent(renderer);
    })
        //Flatten the stream of observable<SDL_Event>s.
        .concat()
        //Delay connecting to the underlying event stream until we're ready.
        .publish();

    //Handle Quit messages. Pretty importaint: without this the exit button doesn't work.
    eventStream
        .filter([](SDL_Event e) {return e.type == SDL_QUIT; }) //When we get an SDL_QUIT message.
        .subscribe([&quit](SDL_Event e) {  quit = true; }); //trigger the event loop completing after the next render. 

    //Set up the mouse button press pipeline.
    eventStream
        //when we press the left mouse button.
        .filter([](SDL_Event e) {return e.type == SDL_MOUSEBUTTONDOWN&&e.button.button == SDL_BUTTON_LEFT; })
        //Change the trail color to red.
        .subscribe([&red, &green, &blue](SDL_Event e) { {
                red = 255;
                blue = 0;
                green = 0;
            }});

    //Set up the mouse button release pipeline.
    eventStream
        //when we release the left mouse button
        .filter([](SDL_Event e) {return e.type == SDL_MOUSEBUTTONUP&&e.button.button == SDL_BUTTON_LEFT; })
        //Change the trail color to white.
        .subscribe([&red, &green, &blue](SDL_Event e) { {
                red = 255;
                blue = 255;
                green = 255;
            }});

    //Set up the mouse trail pipeline.
    auto x = eventStream
        //When we move the mouse
        .filter([](SDL_Event e) {return e.type == SDL_MOUSEMOTION; })
        //Get the new position of the mouse. 
        .map([](SDL_Event e) {SDL_Point p = { e.motion.x, e.motion.y }; return std::move(p); })
        //Buffer these positions.
        .buffer(TRAIL_LENGTH, 1)
        //Copy each buffer into the points array for rendering. 
        .subscribe([&points](std::vector<SDL_Point> values) { {
                int idx = 0;
                for (int i = 0; i < values.size(); i++) {
                    points[i] = values.at(i);
                }
            }});

    //Connect to the event stream and begin pumping events down the pipeline.
    //Note: Without modification, this method will "block" until the eventStream is complete. 
    auto connectionSub = eventStream.connect();

    //Clean up SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(windowMain);
    SDL_Quit();

    //Return 0
    return 0;

}