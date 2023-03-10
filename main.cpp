//********************************************************
//* Name: Xander Doom and AJ Varchetti     Date: 12/8/21 *
//* Seats: 13 and 14                       File: SDP     *
//* Instructor: PAC                        Time: 8:00    *
//********************************************************

//------------
// LIBRARIES
//------------
#include "FEHLCD.h"
#include "FEHUtility.h"
#include "FEHImages.h"
#include "FEHRandom.h"
#include "FEHSD.h"

// Used for vector shenanigans
#include "vector"
#include "functional"
#include "algorithm"
#include "cmath"

//-------------------------
// DEFINITIONS / VARIABLES
//-------------------------
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define CAR_WIDTH1 16
#define LOG_WIDTH1 48
#define LOG_WIDTH2 96
#define LOG_WIDTH3 64

#define SCORES_PATH "Scores.dat" // Scores file

//-------------------------
// COLORS
//-------------------------
#define LOG_COLOR 0x924A18
#define WATER_COLOR 0x1042f4
FEHIMAGE SPRITE_FROG, SPRITE_CAR, SPRITE_TURTLE, SPRITE_LOG, SPRITE_ROAD, SPRITE_GRASS, SPRITE_WATER;

// global variables
float difficulty;
int state;

//------------
// CLASSES
//------------

// Scoreboard display
class Scoreboard
{
private:
    float score;
    char cscore[20], chighscore[20];
    int old_score, highscore;
    std::vector<int> old_scores;

public:
    Scoreboard(void)
    {
        score = 0;
    }
    void AddRow()
    {
        score += 1000 * difficulty * difficulty;
    }
    void RemTime(float dt)
    {
        score -= 200 * dt;
        if (score < 0)
            score = 0; // Keep score from going below zero
    }
    void RemRow(void)
    {
        score -= 1000 * difficulty * difficulty;
    }
    float GetScore(void)
    {
        return score;
    }
    float GetHighScore(void)
    {
        return highscore;
    }
    float GetGamesPlayed(void)
    {
        return old_scores.size() - 1;
    }
    void Draw(void)
    {
        LCD.SetFontColor(WHITE);
        sprintf(cscore, "Score: %07d", int(score));
        sprintf(chighscore, "Highscore: %07d", highscore);
        if (int(score) > highscore)
            LCD.SetFontColor(GOLD);
        LCD.WriteAt(cscore, SCREEN_WIDTH - 174, 26);
        if (int(score) > highscore)
            LCD.SetFontColor(WHITE);
        LCD.WriteAt(chighscore, SCREEN_WIDTH - 222, 6);
    }
    void Reset(void)
    {
        score = 0;
    }
    void Load(const char file_path[99])
    {
        old_scores.clear(); // Wipe the current vector

        FEHFile *highscores_in = SD.FOpen(file_path, "r"); // Open for reading
        int tmp = 0;
        while (!SD.FEof(highscores_in)) // While there are new scores to scan in
        {
            SD.FScanf(highscores_in, "%d", &tmp); // Scan the score
            old_scores.push_back(tmp);            // Add the score to the list
        }
        SD.FClose(highscores_in);  // Close the input file
        if (old_scores.size() > 0) // Avoid out of range refrencing
        {
            highscore = *std::max_element(old_scores.begin(), old_scores.end()); // Loop through vector and find the max
        }
    }
    void Save(const char file_path[99])
    {
        if (score > 0)
        {
            FEHFile *highscores_out = SD.FOpen(file_path, "a"); // Open file for appending
            SD.FPrintf(highscores_out, "%d\n", int(score));     // Write the current score to the file
            SD.FClose(highscores_out);                          // Close the output file
        }
    }
};

// Menu display object
class Menu
{
public:
    Menu()
    {
        // State of the game. 0: main menu, 1: playing the game, 2: displaying statistics, 3:displaying instructions, 4: quitting
        state = 0;
    }
    void Draw(int, float, float, Scoreboard *);
    int Update(int, int, float x, float y);
};

// Object with spacial coordinates, a horizontal velocity, width, and height
class Entity
{
public:
    Entity(float x, float y, float v, float w, float h = TILE_HEIGHT)
    {
        xpos = x;                  // px
        ypos = y;                  // px
        velocity = difficulty * v; // px/sec
        width = w;                 // px
        height = h;                // px
    }
    void Update(float);
    float getXpos();
    float getYpos();
    float getWidth();
    float getVelocity();
    virtual void Draw(int){};
    virtual ~Entity(){};

protected:
    // Xpos and Ypos correspond to the coordinates of the tile the Entitys is located. (0,0) is the top left corner.
    float xpos, ypos;
    // Speed at which the Entitys is moving. Positive number left -> right. Negative number for right -> left.
    float velocity;
    // Width and Height represent the bounding box of the object, and can be used to draw a simple representation
    float width, height;
};

// Row object. Holds pointers to obstacles and background entities
class Row
{
public:
    std::vector<Entity *> getEntities();

    // Update all objects in the row
    void Update(float dt)
    {
        for (Entity *e : row_elements)
        { // update all elements in the world_elements vector
            e->Update(dt);
        }
    }

    virtual void Draw(int row) // Draw all objects in the row
    {
        for (Entity *e : row_elements)
        { // update all elements in the world_elements vector
            e->Draw(row);
        }
    }

    void AddElement(Entity *elem) // Add an object to a row by pointer
    {
        row_elements.push_back(elem); //"Push" the pointer "elem" to the "back" of the vector
    }

    void RemElement(Entity *elem) // Remove an obstacle by pointer
    {
        row_elements.erase(std::remove(row_elements.begin(), row_elements.end(), elem), row_elements.end()); // Remove pointer "elem" from vector of pointers to rows                                                                              // free elem from memory
    }

    void DelElement(Entity *elem) // Remove AND DELETE an obstacle by pointer //!Might never need to do this
    {
        row_elements.erase(std::remove(row_elements.begin(), row_elements.end(), elem), row_elements.end()); // Remove pointer "elem" from vector of pointers to rows
        delete elem;                                                                                         // free elem from memory
    }

    virtual ~Row()
    { // If a row is deleted, make sure to delete all of its contained objects too
        for (Entity *e : row_elements)
        { // update all elements in the row_elements vector
            delete e;
        }
    }

private:
    std::vector<Entity *> row_elements; // list of things like the background and any obstacles
};

// Game state object. Amalgamation of all the rows and other entities required to make the game run. (besides the frog)
// Basically a big nested list
class World
{
public:
    void Update(int, float);                         // Update all rows in the frame
    void Draw(int);                                  // Draw all rows in the frame
    void addToRow(int, Entity *);                    // Adds an entity object to the desired row
    void removeFromRow(int, Entity *);               // Removes an entity object to the desired row
    Entity *checkCollision(int row, Entity *target); // Checks for collisions of a target entity with all entities in a row. Returns the pointer if theres a collision, otherwise NULL
    const char *GetRowType(int row)
    {
        return typeid(*world_elements.at(row)).name();
    }

    void AddRow(Row *elem) // Add a row at the top of the screen by pointer
    {
        world_elements.push_back(elem); //"Push" the pointer "elem" to the "back" of the vector
    }

    void Generate(int new_total_rows); // Add random rows up to a passed number

    void RemRow(Row *elem) // Add a row by pointer
    {
        world_elements.erase(std::remove(world_elements.begin(), world_elements.end(), elem), world_elements.end()); // Remove pointer "elem" from vector of pointers to rows
        delete elem;                                                                                                 // free elem from memory
    }

    void Reset()
    {
        world_elements.clear();
    }

    ~World() // If the gamestate is deleted, make sure to delete all of the rows too
    {
        for (Row *e : world_elements)
        { // update all elements in the world_elements vector
            delete e;
        }
    }

private:
    std::vector<Row *> world_elements;
};

// Frog class, Main Entity
class Frog : public Entity
{
    // TODO:
public:
    Frog(int x, int y, float v, float w) : Entity(x, y, v, w) {}
    void Draw(int row);
    void Move(float, float);
    void Reset()
    {
        xpos = SCREEN_WIDTH / 2;
    }
};

// Turtle Class, Obstacle in Water
class Turtle : public Entity
{
    // TODO:
public:
    Turtle(int x, float v, float w, float h = 14.0) : Entity(x, 0, v, w, h) {}
    void Draw(int row);
};

// Log Class, Obstacle in Water
class Log : public Entity
{
    // TODO:
public:
    Log(int x, float v, float w, float h = 14.0) : Entity(x, 0, v, w, h) {}
    void Draw(int row);
};

// Car Class, Obstacle on Road
class Car : public Entity
{
    // TODO:
public:
    Car(int x, int y, float v, float w) : Entity(x, y, v, w) {}
    void Draw(int row);
};

// Road Class, Type of Row in World
// Contains Car Entities / Objects
class Road : public Row
{
    // TODO:
public:
    Road(int type) : Row()
    {
        // todo Add car randomization (using int type)
        AddElement(new Car(Random.RandInt() % SCREEN_WIDTH, 0, 2, CAR_WIDTH1));   //! TESTING
        AddElement(new Car(Random.RandInt() % SCREEN_WIDTH, 0, 20, CAR_WIDTH1));  //! TESTING
        AddElement(new Car(Random.RandInt() % SCREEN_WIDTH, 0, 240, CAR_WIDTH1)); //! TESTING
    }
    void Draw(int row); // Row at which to draw the background (0 = bottom row)
};

// Grass Class, Type of Row in World
// Safe area for frog
class Grass : public Row
{
    // TODO:
public:
    Grass() : Row() {}
    void Draw(int row);
};

// Water Class, Type of Row in World
// Contains Logs and Turtles
class Water : public Row
{
    // TODO:
public:
    Water(int type) : Row()
    {
        if (type == 0)
        { // If type zero is passed, randomize the type
            type = (Random.RandInt() % 4) + 1;
        }
        int turtle_offset = Random.RandInt() % 16;
        int x = Random.RandInt() % 120 + 30; // LOG SPEED
        if (Random.RandInt() % 2)            // 50-50 Positive, Negative Velocity
        {
            x = x * -1;
        }
        switch (type)
        {
        case 1:
            // Water1 type
            AddElement(new Log(Random.RandInt() % (SCREEN_WIDTH / 2), x, LOG_WIDTH1));                    // Add Log Entity to Water
            AddElement(new Log(Random.RandInt() % (SCREEN_WIDTH / 2) + SCREEN_WIDTH / 2, x, LOG_WIDTH1)); // Add Log Entity to Water
            break;
        case 2:
            // Water2 type
            AddElement(new Log(Random.RandInt() % (SCREEN_WIDTH / 2), x, LOG_WIDTH2)); // Add Log Entity to Water
            break;
        case 3:
            // Water3 type
            AddElement(new Log(Random.RandInt() % (SCREEN_WIDTH / 2), x, LOG_WIDTH3));                    // Add Log Entity to Water
            AddElement(new Log(Random.RandInt() % (SCREEN_WIDTH / 2) + SCREEN_WIDTH / 2, x, LOG_WIDTH3)); // Add Log Entity to Water
            break;
        case 4:                                                         // TBA types
            AddElement(new Turtle(16 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(32 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(48 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water

            AddElement(new Turtle(128 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(144 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(160 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water

            AddElement(new Turtle(240 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(256 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            AddElement(new Turtle(272 + turtle_offset, 40, TILE_WIDTH)); // Add Turtle Entity to Water
            break;
        }
    }
    void Draw(int row);
};

//---------------------
// Function Prototypes
//---------------------
void getDifficulty();
int getUserInput(float, float, Entity *);
void endGame(Scoreboard *, World *, int *, Frog *);

//----------------------
// Main Method
//----------------------
int main()
{
    // Declare variables for use throughout main
    float touchx, touchy;
    bool touched = 0, touched_last_frame = 0;
    int frog_row = 1;
    int move;
    float frog_distance = 160;
    Entity *collided_object = NULL;
    Frog *frog = new Frog(frog_distance, SCREEN_HEIGHT - (3 * TILE_HEIGHT), 0, TILE_WIDTH);

    // Create persistent objects
    World world = World();
    Menu main_menu = Menu();
    Scoreboard scoreboard = Scoreboard();

    // Load sprites
    SPRITE_FROG.Open("FrogFEH.pic");
    SPRITE_CAR.Open("CarFEH.pic");
    SPRITE_TURTLE.Open("TurtleFEH.pic");
    SPRITE_LOG.Open("LogFEH.pic");
    SPRITE_ROAD.Open("RoadFEH.pic");
    SPRITE_GRASS.Open("GrassFEH.pic");
    SPRITE_WATER.Open("WaterFEH.pic");

    // Load scores
    scoreboard.Load(SCORES_PATH);

    // Get inital game time //todo make this a function probably
    int current_frame_time = 0, prev_frame_time = 0; // Intermediary calculation variables for the frame_time (msecs)
    float frame_time;                                // Time it took for the last frame to render (seconds)

    // Initalize the world with the three starting rows
    world.AddRow(new Grass);
    world.AddRow(new Grass);
    world.AddRow(new Grass);

    // Add frog to starting row
    world.addToRow(2, frog);

    // Infinite update loop
    while (1)
    {
        // Time updates
        prev_frame_time = current_frame_time;
        current_frame_time = TimeNowMSec();
        frame_time = (current_frame_time - prev_frame_time) / 1000.; // Calculate the time the last frame took (in ms) for velocity calculations

        // Prevent touchscreen input from freezing the program or repeating on a held mouse press
        if (touched)
            touched_last_frame = true;
        else
            touched_last_frame = false;
        // Get the user input for this loop (only once instead of calling it in each method)
        touched = LCD.Touch(&touchx, &touchy);

        // Update the menu if the user clicks
        if (touched && !touched_last_frame)
        {
            if (main_menu.Update(touched, touched && !touched_last_frame, touchx, touchy))
            {
                endGame(&scoreboard, &world, &frog_row, frog);
            }
        }

        switch (state) // Switch case for the state of the game
        {

        case 1: //* Main GAME functionality start //

            //------------------------------------------
            // Get user input
            //------------------------------------------

            if (touched && !touched_last_frame)
            {
                move = getUserInput(touchx, touchy, frog);
                world.removeFromRow(frog_row, frog);

                switch (move)
                {

                case 1:
                    // Move frog up if click is above frog, update score
                    frog_row++;
                    scoreboard.AddRow();
                    break;
                case 2:
                    // Move frog right if click is right of frog
                    if (frog->getXpos() < SCREEN_WIDTH - TILE_WIDTH)
                    {
                        frog->Move(TILE_WIDTH, 0);
                    }
                    break;
                case 3:
                    // Move frog down if click is below frog, update score
                    if (frog_row >= 3)
                    {
                        frog_row--;
                        scoreboard.RemRow();
                    }
                    break;
                case 4:
                    // Move frog left if click is left of frog
                    if (frog->getXpos() > TILE_WIDTH - 1)
                    {
                        frog->Move(-TILE_WIDTH, 0);
                    }
                    break;
                }
                world.addToRow(frog_row, frog);
            }

            //------------------------------------------
            // Calculations / updates
            //------------------------------------------

            // Set the scoreboard to zero if the player is at the start
            if (frog_row <= 3)
                scoreboard.Reset();

            // Generate new rows based on the frog's position
            world.Generate(frog_row + 12); // 12 is the number of frog rows

            collided_object = world.checkCollision(frog_row, frog); // Run collision logic and return a pointer to any object the frog collides with

            if (world.GetRowType(frog_row) == typeid(Water).name()) // If the frog is in a water row
            {
                if (collided_object == NULL)
                { // Water collision

                    // Do normal drawing tasks
                    LCD.Clear();
                    world.Draw(frog_row - 2);
                    scoreboard.Draw();

                    endGame(&scoreboard, &world, &frog_row, frog);
                }
                else if (typeid(*collided_object).name() == typeid(Log).name()) // Collision with a log
                {
                    if (!(frog->getXpos() < 1) && !(frog->getXpos() > (SCREEN_WIDTH - frog->getWidth())))
                    {
                        frog->Move(collided_object->getVelocity() * frame_time, 0); // move the frog at the speed of the log
                    }
                }
                else if (typeid(*collided_object).name() == typeid(Turtle).name()) // Collision with a turtle
                {
                    if (!(frog->getXpos() < 1) && !(frog->getXpos() > (SCREEN_WIDTH - frog->getWidth())))
                    {
                        frog->Move(collided_object->getVelocity() * frame_time, 0); // move the frog at the speed of the turtle
                    }
                }
            }
            else if (collided_object == NULL)
            {
            }    // No collision
            else // Collision with anything else
            {
                // Do normal drawing tasks
                LCD.Clear();
                world.Draw(frog_row - 2);
                scoreboard.Draw();

                endGame(&scoreboard, &world, &frog_row, frog);
            }

            world.Update(frog_row - 2, frame_time); // Update all rows on screen
            scoreboard.RemTime(frame_time);         // Update the scoreboard based on the time on screen

            //------------------------------------------
            // Display / Draws
            //------------------------------------------
            // This is done seprately from the calculations to reduce screen flicker

            LCD.Clear();
            // Draw all rows on screen, starting with the frog row
            world.Draw(frog_row - 2); //+1 is the distance of the frog from the bottom of the screen

            break; //* Main GAME functionality end //

        case 0:
            // Menu is displayed elsewhere
            LCD.Clear();
            break;

        case 2:
            LCD.Clear();
            // todo Display statistics
            break;

        case 3:
            LCD.Clear();
            // todo Display instructions
            break;

        case 4:
            LCD.Clear();
            // todo Display credits
            break;
        case 5:
            LCD.Clear();
            // todo Get difficulty
        }

        // Update menu (scoreboard is passsed for statistics screen display)
        main_menu.Draw(touched, touchx, touchy, &scoreboard);
    }
}

//----------------------
// Functions / Methods
//----------------------

void Entity::Update(float dt)
{ // Update the x value of the entity (positive velocity for rightward movement, negative for leftward)
    xpos = fmod(SCREEN_WIDTH + xpos + dt * velocity, SCREEN_WIDTH);
}

// Draw Water Row in row
void Water::Draw(int row)
{
    LCD.SetFontColor(WATER_COLOR);
    LCD.FillRectangle(0, SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, SCREEN_WIDTH, TILE_HEIGHT);
    for (int i = 0; i < SCREEN_WIDTH / TILE_WIDTH; i++)
    {
        SPRITE_WATER.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, i * TILE_WIDTH); // Draw water sprite
    }
    Row::Draw(row);
}

// Draw Road Row in row
void Road::Draw(int row)
{
    LCD.SetFontColor(GRAY);
    LCD.FillRectangle(0, SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, SCREEN_WIDTH, TILE_HEIGHT);
    for (int i = 0; i < SCREEN_WIDTH / TILE_WIDTH; i++)
    {
        SPRITE_ROAD.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, i * TILE_WIDTH); // Draw road sprite
    }
    Row::Draw(row);
}

// Draw Grass Row in row
void Grass::Draw(int row)
{
    LCD.SetFontColor(GREEN);
    LCD.FillRectangle(0, SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, SCREEN_WIDTH, TILE_HEIGHT);
    for (int i = 0; i < SCREEN_WIDTH / TILE_WIDTH; i++)
    {
        SPRITE_GRASS.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, i * TILE_WIDTH); // Draw grass sprite
    }
    Row::Draw(row);
    Row::Draw(row);
}

// Draw Car Entity in row
void Car::Draw(int row)
{
    SPRITE_CAR.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT + 1, xpos);
}

// Draw Log Entity in row
void Log::Draw(int row)
{
    LCD.SetFontColor(LOG_COLOR);
    LCD.FillRectangle(xpos + 1, SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT + 1, width - 1, height); // Still draw the rectangle as a fallback for weird sprite behavior
    for (int i = 0; i < int(width / TILE_WIDTH); i++)
    {
        SPRITE_LOG.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, xpos + i * TILE_WIDTH); // Draw log sprite
    }
}

// Draw Frog Entity in row
void Frog::Draw(int row)
{
    SPRITE_FROG.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT + 1, xpos + 1);
}

// Draw Turtle Entity in row
void Turtle::Draw(int row)
{
    SPRITE_TURTLE.Draw(SCREEN_HEIGHT - (row + 1) * TILE_HEIGHT, xpos);
}

// Draw World, given start row
void World::Draw(int start_row)
{
    if ((start_row < int(world_elements.size())) && start_row >= 0) // Ensure there's no out-of-index refrencing
    {
        int i = 0;
        for (std::vector<Row *>::iterator it = world_elements.begin() + start_row; it != world_elements.end(); it++)
        { // Starting at the given index, draw rows bottom up //todo Add limit based on screen height in tiles
            (*it)->Draw(i);
            if (i++ > 10)
                break; // Iterate and also only render elements on the screen
        }
    }
    else
    {                               // If something tries to draw an invalid array index, display a pink background instead as an error
        LCD.SetFontColor(0xff00ff); // ERROR COLORING
        LCD.FillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

// Update world, and all position
void World::Update(int start_row, float dt)
{                                                                   // Basically the same as the draw function but it updates things instead :)
    if ((start_row < int(world_elements.size())) && start_row >= 0) // Ensure there's no out-of-index refrencing
    {
        int i = 0;
        for (std::vector<Row *>::iterator it = world_elements.begin() + start_row; it != world_elements.end(); it++)
        { // Starting at the given index, draw rows bottom up
            (*it)->Update(dt);
            if (i++ > 10)
                break; // Iterate and also only calculate elements on the screen
        }
    }
    else
    {                               // If something tries to update an invalid array index, display a pink background instead as an error
        LCD.SetFontColor(0xff00ff); // ERROR COLORING
        LCD.FillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

// Add entity to to row
void World::addToRow(int currentRow, Entity *add)
{
    world_elements.at(currentRow)->AddElement(add);
}

// Create a row of a random type of Row at the top of the screen based on the frog position
void World::Generate(int new_rows_total)
{
    int new_num_rows;
    while (int(world_elements.size()) < new_rows_total)
    { // Add rows until theres enough

        if ((Random.RandInt() % 2))
        {                                            // 0.5 chance of road
            new_num_rows = Random.RandInt() % 4 + 2; // Add between 2 and 5 road tiles
            for (int i = 0; i < new_num_rows; i++)
            {
                World::AddRow(new Road(0));
            }
        }
        else
        {                                            // 0.5 chance of road
            new_num_rows = Random.RandInt() % 4 + 2; // Add between 2 and 5 water tiles
            for (int i = 0; i < new_num_rows; i++)
            {
                World::AddRow(new Water(0));
            }
        }

        World::AddRow(new Grass()); // Terminate with a grass row every time
    }
}

// Remove Entity from Row
void World::removeFromRow(int currentRow, Entity *add)
{
    world_elements.at(currentRow)->RemElement(add);
}

// Check collision between frog and any other entity. Returns the pointer to the entity it collides with.
Entity *World::checkCollision(int currentRow, Entity *check)
{

    for (Entity *e : world_elements.at(currentRow)->getEntities()) // For every entity in the row
    {
        if (!(e == check)) // besides the check element
        {
            if (typeid(*e).name() == typeid(Log).name() || (typeid(*e).name() == typeid(Turtle).name()))
            {
                if (e->getXpos() - check->getXpos() < check->getWidth() && e->getXpos() - check->getXpos() > 0) // If it overlaps
                {
                    return e;
                }

                if (e->getVelocity() < 0 || e->getVelocity() > 0)
                {
                    if (check->getXpos() - (e->getXpos() - SCREEN_WIDTH) < e->getWidth() && check->getXpos() - (e->getXpos() - SCREEN_WIDTH) > 0) // If it overlaps
                    {
                        return e;
                    }
                }
            }

            if (e->getXpos() - check->getXpos() < check->getWidth() && e->getXpos() - check->getXpos() > 0) // If it overlaps
            {
                return e;
            }

            if (check->getXpos() - e->getXpos() < e->getWidth() && check->getXpos() - e->getXpos() > 0) // If it overlaps
            {
                return e;
            }
        }
    }
    return NULL;
}

// Returns a vector of Entity*
std::vector<Entity *> Row::getEntities()
{
    return row_elements;
}

// Return X position of an Entity
float Entity::getXpos()
{
    return xpos;
}

// Return Y position of an Entity
float Entity::getYpos()
{
    return ypos;
}

// Return Width of an Entity
float Entity::getWidth()
{
    return width;
}

// Return Velocity of an Entity
float Entity::getVelocity()
{
    return velocity;
}

// Moves frog
void Frog::Move(float x, float y)
{
    xpos += x;
    ypos += y;
}

// Draws Menu Screen
void Menu::Draw(int touched, float x, float y, Scoreboard *scoreboard_ptr)
{
    LCD.SetFontColor(WHITE);
    if (state == 0)
    {
        if (((60 <= x) && SCREEN_WIDTH - 60 > x))
        {
            LCD.SetFontColor(0x555555); // Button hover highlighting
            if (((80 <= y) && 100 > y))
            { // button1
                LCD.FillRectangle(60, 77, 200, 21);
            }
            else if (((100 <= y) && 120 > y))
            { // button2
                LCD.FillRectangle(60, 97, 200, 21);
            }
            else if (((120 <= y) && 140 > y))
            { // button3
                LCD.FillRectangle(60, 117, 200, 21);
            }
            else if (((140 <= y) && 160 > y))
            { // button4
                LCD.FillRectangle(60, 137, 200, 21);
            }
        }

        LCD.SetFontColor(LIMEGREEN);
        LCD.WriteAt("BOGGER!", 61, 60); // Draw title

        LCD.SetFontColor(WHITE); // Draw buttons
        LCD.DrawRectangle(60, 77, 200, 21);
        LCD.WriteAt("Play Game", 61, 80);
        LCD.DrawRectangle(60, 97, 200, 21);
        LCD.WriteAt("Stats", 61, 100);
        LCD.DrawRectangle(60, 117, 200, 21);
        LCD.WriteAt("Instructions", 61, 120);
        LCD.DrawRectangle(60, 137, 200, 21);
        LCD.WriteAt("View Credits", 61, 140);
    }

    else if (state == 1) // Game state
    {
        // Display current score
        scoreboard_ptr->Draw();
    }
    else if (state == 2) // Stats state
    {

        LCD.SetFontColor(WHITE);
        // char cstats[20];
        LCD.WriteAt("STATISTICS:", 61, 60);
        char tmpstr[20];
        sprintf(tmpstr, "Highscore: %07d", int(scoreboard_ptr->GetHighScore()));
        LCD.WriteAt(tmpstr, 61, 100);
        sprintf(tmpstr, "Games played:%5d", int(scoreboard_ptr->GetGamesPlayed()));
        LCD.WriteAt(tmpstr, 61, 120);
        LCD.DrawRectangle(59, 56, 223, 83);
    }
    else if (state == 3)
    { // Instructions state

        LCD.WriteAt("INSTRUCTIONS:", 3, 40);
        LCD.WriteAt(" ", 7, 60);
        LCD.WriteAt("The frog will move in the", 7, 80);
        LCD.WriteAt("direction of your mouse.", 7, 100);
        LCD.WriteAt(" ", 7, 120);
        LCD.WriteAt("The objective is to move", 7, 140);
        LCD.WriteAt("the frog from one side of", 7, 160);
        LCD.WriteAt("the map to the other", 7, 180);
        LCD.WriteAt("without hitting anything", 7, 200);
        LCD.WriteAt("or falling in the water.", 7, 220);
    }
    else if (state == 4)
    { // Credits tate
        LCD.DrawRectangle(59, 56, 202, 103);
        LCD.WriteAt("CREDITS: ", 61, 60);
        LCD.WriteAt("AJ Varchetti", 61, 80);
        LCD.WriteAt("Xander Doom", 61, 100);
        LCD.WriteAt("1281.02H: FEH", 61, 120);
        LCD.WriteAt("PAC  8:00", 61, 140);
    }
    else if (state == 5)
    {
        if (((60 <= x) && SCREEN_WIDTH - 60 > x))
        {
            if ((77 <= y) && 98 > y)
            {
                LCD.SetFontColor(GRAY);
                LCD.FillRectangle(60, 77, 200, 21);
            }

            else if ((97 <= y) && 118 > y)
            {
                LCD.SetFontColor(GRAY);
                LCD.FillRectangle(60, 97, 200, 21);
            }

            else if ((117 <= y) && 138 > y)
            {
                LCD.SetFontColor(GRAY);
                LCD.FillRectangle(60, 117, 200, 21);
            }

            else if ((137 <= y) && 158 > y)
            {
                LCD.SetFontColor(GRAY);
                LCD.FillRectangle(60, 137, 200, 21);
            }
        }

        // Draw the Difficulty Screen
        LCD.SetFontColor(WHITE);
        LCD.WriteAt("Difficulty:", 61, 60);
        LCD.DrawRectangle(60, 77, 200, 21);
        LCD.WriteAt("Easy", 61, 80);
        LCD.DrawRectangle(60, 97, 200, 21);
        LCD.WriteAt("Medium", 61, 100);
        LCD.DrawRectangle(60, 117, 200, 21);
        LCD.WriteAt("Hard", 61, 120);
        LCD.DrawRectangle(60, 137, 200, 21);
        LCD.WriteAt("Harder :)", 61, 140);
    }
    if (state != 0)
    { // For all states besides the menu, draw the return button
        if (((3 <= x) && 83 > x))
        {
            if ((3 <= y) && 24 > y)
            {
                LCD.SetFontColor(GRAY);
                LCD.FillRectangle(3, 3, 80, 22);
                LCD.SetFontColor(WHITE);
            }
        }
        LCD.DrawRectangle(3, 3, 80, 22);
        LCD.WriteAt("Return", 4, 7);
    }
}

// Updates Menu
int Menu::Update(int touched, int NewTouched, float x, float y)
{

    if (touched)
    {
        if (state == 0) // Main menu state
        {
            if (((60 <= x) && SCREEN_WIDTH - 60 > x))
            {
                if (((80 <= y) && 100 > y))
                { // button1
                    if (NewTouched)
                        state = 5;
                }
                else if (((100 <= y) && 120 > y))
                { // button2
                    if (NewTouched)
                        state = 2;
                }
                else if (((120 <= y) && 140 > y))
                { // button3
                    if (NewTouched)
                        state = 3;
                }
                else if (((140 <= y) && 160 > y))
                { // button4
                    if (NewTouched)
                        state = 4;
                }
            }
        }
        else if (state == 1) // Return button for gameplay state
        {
            if (state != 0)
            { // For all states besides the menu, update the return button
                if (((3 <= x) && 83 > x))
                {
                    if ((3 <= y) && 24 > y)
                    {
                        state = 0;
                        return 1;
                    }
                }
            }
        }

        else if (state == 5) // Get difficulty state
        {
            if (((60 <= x) && SCREEN_WIDTH - 60 > x))
            {
                if ((77 <= y) && 98 > y)
                {
                    difficulty = 0.4; // Set the difficulty to easy
                    state = 1;        // set to game running state
                }

                else if ((97 <= y) && 118 > y)
                {
                    difficulty = 0.8; // Set the difficulty to medium
                    state = 1;        // set to game running state
                }

                else if ((117 <= y) && 138 > y)
                {
                    difficulty = 1.3; // Set the difficulty to hard
                    state = 1;        // set to game running state
                }

                else if ((137 <= y) && 158 > y)
                {
                    difficulty = 2.0; // Set the difficulty to harder :)
                    state = 1;        // set to game running state
                }
            }
        }

        if (state != 0)
        { // For all states besides the menu, update the return button
            if (((3 <= x) && 83 > x))
            {
                if ((3 <= y) && 24 > y)
                {
                    state = 0;
                }
            }
        }
    }
    return 0;
}

// Takes the click location and entity pointer, then determines the direction the frog moves based on the location of the click relative to the frog
// Returns 1 for up, 2 for right, 3 for down, 4 for left
int getUserInput(float x, float y, Entity *e)
{

    // line1 is -3/4x + line1b. True is above the line, false is below.
    // line 2 is 3/4x + line2b
    bool line1, line2;

    // Y-intercept of the lines through the frog
    float line1b = e->getYpos() - (-.75 * e->getXpos()) + 8;
    float line2b = e->getYpos() - (.75 * e->getXpos()) + 8;

    // If above line1
    if (y >= -3.0 / 4 * (x - 8) + line1b)
    {
        line1 = true;
    }
    else
    {
        line1 = false;
    }

    // If above line2
    if (y >= 3.0 / 4 * (x - 8) + line2b)
    {
        line2 = true;
    }
    else
    {
        line2 = false;
    }

    // Determines what zone the touch was in.
    if (line1 && line2)
    {
        return 3; // Zone 3
    }
    else if (!line1 && line2)
    {
        return 4; // Zone 4
    }
    else if (line1 && !line2)
    {
        return 2; // Zone 2
    }
    else
    {
        return 1; // Zone 1
    }
}

// Ends game and resets the game to be playable again.
void endGame(Scoreboard *scoreboard_ptr, World *world_ptr, int *frog_row_ptr, Frog *frog_ptr)
{

    int x;
    int y;

    // Set the program state back to the main menu
    // state = 0;

    // Save and reset the scoreboard
    scoreboard_ptr->Save(SCORES_PATH); // Save the current score
    scoreboard_ptr->Reset();           // Reset the scoreboard
    scoreboard_ptr->Load(SCORES_PATH); // Reload the number of games and highscores

    LCD.SetFontColor(RED);
    LCD.WriteAt("GAME OVER", 12, 26);
    // sprintf(cscore, "Score: %07d", int(score));
    // LCD.WriteAt(cscore, 61, 80);

    world_ptr->Reset();
    // Initalize the world with the three starting rows
    world_ptr->AddRow(new Grass());
    world_ptr->AddRow(new Grass());
    world_ptr->AddRow(new Grass());
    world_ptr->AddRow(new Grass());
    world_ptr->AddRow(new Grass());

    *frog_row_ptr = 2; // Reset the frog's position
    frog_ptr->Reset();
    world_ptr->addToRow(2, frog_ptr);

    Sleep(0.2); // Pause the program so the menu isn't instantly dismissed

    while (LCD.Touch(&x, &y))
        ; // Let go
    while (!LCD.Touch(&x, &y))
        ; // freeze until user clicks
}