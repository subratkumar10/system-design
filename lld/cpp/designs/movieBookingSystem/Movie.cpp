#include "Movie.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace std;


Movie::Movie (  string id, 
        string title,
        vector<string> casts, 
        MovieGenre moviegenre,
         int duration,
         string language
    ) : id(id) , title(title), casts(casts), moviegenre(moviegenre), duration(duration), language(language){}



std::string Movie::getMovieId() const { return id; }
std::string Movie::getTitle() const { return title; }
MovieGenre Movie::getGenre() const { return moviegenre; }
int Movie::getDurationMinutes() const { return duration; }
std::string Movie::getLanguage() const { return language; }

const std::vector<std::string>& Movie::getCast() const { return casts; }

void Movie::addCastMember(const std::string& actor) {
    casts.push_back(actor);
}

void Movie::displayInfo()
{

    switch(moviegenre) {
        case MovieGenre::ACTION : {
            cout<<"Action movie"<<endl;
            break;
        }
        case MovieGenre::ROMANCE : {
            cout<<"ROMANCE movie"<<endl;
            break;
        }
        case MovieGenre::THRILLER : {
             cout<<"THRILLER movie"<<endl;
             break;
        }
        case MovieGenre::COMEDY : {

            cout<<"COMEDY movie"<<endl;
            break;

        }
        default : {
            cout<<"No movie type"<<endl;
        }
    }

}
