#pragma once

#include<iostream>
#include<string>
#include<vector>
using namespace std;

enum MovieGenre {
    ACTION,
    ROMANCE,
    THRILLER,
    COMEDY
};

class Movie {

    private:
    string id;
    string title;
    vector<string> casts;
    MovieGenre moviegenre;
    int duration;
    std::string language;

    public:
    Movie (string id, 
        string title,
        vector<string> casts, 
        MovieGenre moviegenre,
         int duration,
         std::string language
    );


    //getter lines
      string getMovieId() const;
    string getTitle() const;
    MovieGenre getGenre() const;
    int getDurationMinutes() const;
    string getLanguage() const;
    const vector<std::string>& getCast() const;
    void addCastMember(const string& actor);

    void displayInfo();



};

