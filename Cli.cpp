#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/cmdline/CommandLine.hpp>
#include <openrct2/platform/platform.h>
#include <openrct2/rct2/T6Exporter.h>
#include <openrct2/ride/Track.h>
#include <openrct2/TrackImporter.h>

using namespace OpenRCT2;

/*
 * Constants
 */

constexpr char kTrackToLoad[] =
  "/tmp/template.td6";
constexpr char kTrackToSave[] =
  "/tmp/output.td6";
constexpr int kSizeY = 9;
constexpr int kSizeX = 12;
constexpr int kSizeZ = 11;
constexpr int kMinimumTrackSize = 100;
constexpr int kTryPerAttempt = 64000;

/*
 * Types
 */

enum DirectionType { kNorth, kEast, kSouth, kWest };

DirectionType TurnLeft(DirectionType dir);
DirectionType TurnRight(DirectionType dir);

struct Coord {
  // Yes, the order is y, x, z. Don't ask.
  int y;
  int x;
  int z;
};

struct Cell {
  int c00;
  int c01;
  int c10;
  int c11;
};

struct TrackCell {
  Coord coord;
  Cell cell;
};

struct TrackPiece {
  std::vector<TrackCell> shape;
  Coord ptr;
};

struct GeneratorInfo {
  Cell *space;
  std::vector<TrackDesignTrackElement> tracks;
  Coord ptr;
  DirectionType dir;
  std::set<track_type_t> failedTracks;
};

/*
 * Declarations
 */

std::map<track_type_t, DirectionType (*)(DirectionType)> dirStateMachine = {
  {TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES, &TurnRight},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, &TurnRight},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN, &TurnRight},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK, &TurnRight},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP, &TurnRight},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP, &TurnRight},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN, &TurnRight},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN, &TurnRight},
  {TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES, &TurnLeft},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, &TurnLeft},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN, &TurnLeft},
  {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK, &TurnLeft},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP, &TurnLeft},
  {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP, &TurnLeft},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN, &TurnLeft},
  {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN, &TurnLeft},
  // Only used for initial coaster.
  {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES, &TurnRight},
  {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES, &TurnLeft},
};

std::vector<track_type_t> statesForTrackElemFlat = {
  TRACK_ELEM_FLAT,
  TRACK_ELEM_FLAT_TO_LEFT_BANK,
  TRACK_ELEM_FLAT_TO_RIGHT_BANK,
  TRACK_ELEM_FLAT_TO_25_DEG_UP,
  TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP,
  TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP,
  TRACK_ELEM_FLAT_TO_25_DEG_DOWN,
  TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
  TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN,
};
std::vector<track_type_t> statesForTrackElementLeftBank = {
  TRACK_ELEM_LEFT_BANK,
  TRACK_ELEM_LEFT_BANK_TO_FLAT,
  TRACK_ELEM_LEFT_BANK_TO_25_DEG_UP,
  TRACK_ELEM_LEFT_BANK_TO_25_DEG_DOWN,
  TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_UP,
  TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
  TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES,
  TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK,
};
std::vector<track_type_t> statesForTrackElementRightBank = {
  TRACK_ELEM_RIGHT_BANK,
  TRACK_ELEM_RIGHT_BANK_TO_FLAT,
  TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP,
  TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN,
  TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_UP,
  TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN,
  TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES,
  TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK,
};
std::vector<track_type_t> statesForTrackElem25DegUp = {
  TRACK_ELEM_25_DEG_UP_TO_FLAT,
  TRACK_ELEM_25_DEG_UP_TO_LEFT_BANK,
  TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK,
  TRACK_ELEM_25_DEG_UP,
  TRACK_ELEM_25_DEG_UP_TO_LEFT_BANKED_25_DEG_UP,
  TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP,
  TRACK_ELEM_25_DEG_UP_TO_60_DEG_UP,
  TRACK_ELEM_LEFT_VERTICAL_LOOP,
  TRACK_ELEM_RIGHT_VERTICAL_LOOP,
};
std::vector<track_type_t> statesForTrackElem25DegUpLeftBanked = {
  TRACK_ELEM_25_DEG_UP_LEFT_BANKED,
  TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_25_DEG_UP,
  TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_LEFT_BANKED_FLAT,
  TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_FLAT,
  TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
  TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
};
std::vector<track_type_t> statesForTrackElem25DegUpRightBanked = {
  TRACK_ELEM_25_DEG_UP_RIGHT_BANKED,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT,
  TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
  TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
};
std::vector<track_type_t> statesForTrackElem60DegUp = {
  TRACK_ELEM_60_DEG_UP_TO_25_DEG_UP,
  TRACK_ELEM_60_DEG_UP,
  TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP,
  TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP
};
std::vector<track_type_t> statesForTrackElem25DegDown = {
  TRACK_ELEM_25_DEG_DOWN_TO_FLAT,
  TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANK,
  TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK,
  TRACK_ELEM_25_DEG_DOWN,
  TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANKED_25_DEG_DOWN,
  TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN,
  TRACK_ELEM_25_DEG_DOWN_TO_60_DEG_DOWN,
};
std::vector<track_type_t> statesForTrackElem25DegDownLeftBanked = {
  TRACK_ELEM_25_DEG_DOWN_LEFT_BANKED,
  TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN,
  TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_LEFT_BANKED_FLAT,
  TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_FLAT,
  TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
  TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
};
std::vector<track_type_t> statesForTrackElem25DegDownRightBanked = {
  TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT,
  TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT,
  TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
  TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
};
std::vector<track_type_t> statesForTrackElem60DegDown = {
  TRACK_ELEM_60_DEG_DOWN_TO_25_DEG_DOWN,
  TRACK_ELEM_60_DEG_DOWN,
  TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN,
  TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN
};

std::map<track_type_t, std::vector<track_type_t>*> trackStateMachine = {
  {TRACK_ELEM_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_FLAT_TO_LEFT_BANK, &statesForTrackElementLeftBank},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANK, &statesForTrackElementRightBank},
  {TRACK_ELEM_FLAT_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_FLAT_TO_25_DEG_DOWN, &statesForTrackElem25DegDown},
  {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_LEFT_BANK, &statesForTrackElementLeftBank},
  {TRACK_ELEM_LEFT_BANK_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_LEFT_BANK_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_LEFT_BANK_TO_25_DEG_DOWN, &statesForTrackElem25DegDown},
  {TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_25_DEG_UP_LEFT_BANKED, &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES, &statesForTrackElementLeftBank},
  {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK, &statesForTrackElementLeftBank},
  {TRACK_ELEM_RIGHT_BANK, &statesForTrackElementRightBank},
  {TRACK_ELEM_RIGHT_BANK_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN, &statesForTrackElem25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_25_DEG_UP_RIGHT_BANKED, &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES,
    &statesForTrackElementRightBank},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK,
    &statesForTrackElementRightBank},
  {TRACK_ELEM_25_DEG_UP_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANK, &statesForTrackElementLeftBank},
  {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK, &statesForTrackElementRightBank},
  {TRACK_ELEM_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP,
    &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_25_DEG_UP_TO_60_DEG_UP, &statesForTrackElem60DegUp},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_LEFT_BANKED_FLAT,
    &statesForTrackElementLeftBank},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
    &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
    &statesForTrackElem25DegUpLeftBanked},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT,
    &statesForTrackElementRightBank},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
    &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
    &statesForTrackElem25DegUpRightBanked},
  {TRACK_ELEM_60_DEG_UP_TO_25_DEG_UP, &statesForTrackElem25DegUp},
  {TRACK_ELEM_60_DEG_UP, &statesForTrackElem60DegUp},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP, &statesForTrackElem60DegUp},
  {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP, &statesForTrackElem60DegUp},
  {TRACK_ELEM_25_DEG_DOWN_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANK, &statesForTrackElementLeftBank},
  {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK, &statesForTrackElementRightBank},
  {TRACK_ELEM_25_DEG_DOWN, &statesForTrackElem25DegDown},
  {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_25_DEG_DOWN_TO_60_DEG_DOWN, &statesForTrackElem60DegDown},
  {TRACK_ELEM_25_DEG_DOWN_LEFT_BANKED, &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN,
    &statesForTrackElem25DegDown},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_LEFT_BANKED_FLAT,
    &statesForTrackElementLeftBank},
  {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
    &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
    &statesForTrackElem25DegDownLeftBanked},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN,
    &statesForTrackElem25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT,
    &statesForTrackElementRightBank},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT, &statesForTrackElemFlat},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
    &statesForTrackElem25DegDownRightBanked},
  {TRACK_ELEM_60_DEG_DOWN_TO_25_DEG_DOWN, &statesForTrackElem25DegDown},
  {TRACK_ELEM_60_DEG_DOWN, &statesForTrackElem60DegDown},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN,
    &statesForTrackElem60DegDown},
  {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN,
    &statesForTrackElem60DegDown},
  {TRACK_ELEM_LEFT_VERTICAL_LOOP, &statesForTrackElem25DegDown},
  {TRACK_ELEM_RIGHT_VERTICAL_LOOP, &statesForTrackElem25DegDown}
};

TrackPiece trackPieceForFlat = {.shape={
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 0}};
TrackPiece trackPieceForFlatTo25DegUp = {.shape={
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 1}};
TrackPiece trackPieceFor25DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 1}};
TrackPiece trackPieceFor25DegUpToFlat = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 0}};
TrackPiece trackPieceFor25DegUpTo60DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 2}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 2}};
TrackPiece trackPieceFor60DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 2}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 3}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 4}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 4}};
TrackPiece trackPieceFor25DegDown = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, -1}};
TrackPiece trackPieceFor25DegDownToFloat = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, 0}};
TrackPiece trackPieceFor25DegDownTo60DegDown = {.shape={
    {.coord={0, 0, -2}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, -2}};
TrackPiece trackPieceFor60DegDown = {.shape={
    {.coord={0, 0, -4}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -3}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -2}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}}, .ptr={1, 0, -4}};
TrackPiece trackPieceForQuarterTurn5Tiles = {.shape={
  {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 0}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 2, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={2, 1, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 2, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 1, 1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 1}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 2, 1}, .cell={0, 0, 1, 0}}, 
    {.coord={2, 1, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 2, 1}, .cell={1, 1, 1, 1}}}, .ptr={2, 3, 0}};
TrackPiece trackPieceForQuarterTurn5Tiles25DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={0, 1, 1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 0, 2}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 1}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 1, 2}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 1, 3}, .cell={1, 0, 1, 1}}, 
    {.coord={2, 1, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 1, 2}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 1, 3}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 2, 2}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 2, 3}, .cell={0, 0, 1, 0}}, 
    {.coord={2, 2, 2}, .cell={1, 1, 1, 1}}, 
    {.coord={2, 2, 3}, .cell={1, 1, 1, 1}}, 
    {.coord={2, 2, 4}, .cell={1, 1, 1, 1}}}, .ptr={2, 3, 4}};
TrackPiece trackPieceForQuarterTurn5Tiles25DegDown = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={0, 1, -1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 0, -1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 0, -2}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 0}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 1, -1}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 1, -2}, .cell={1, 0, 1, 1}}, 
    {.coord={1, 1, -3}, .cell={1, 0, 1, 1}}, 
    {.coord={2, 1, -1}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 1, -2}, .cell={1, 1, 0, 1}}, 
    {.coord={2, 1, -3}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 2, -1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 2, -2}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 2, -3}, .cell={0, 0, 1, 0}}, 
    {.coord={2, 2, -2}, .cell={1, 1, 1, 1}}, 
    {.coord={2, 2, -3}, .cell={1, 1, 1, 1}}, 
    {.coord={2, 2, -4}, .cell={1, 1, 1, 1}}}, .ptr={2, 3, -4}};
TrackPiece trackPieceForQuarterTurn3Tiles = {.shape={
    {.coord={0, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 0}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 1, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 1, 1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 1}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 1, 1}, .cell={1, 1, 0, 1}}}, .ptr={1, 2, 0}};
TrackPiece trackPieceForQuarterTurn3Tiles25DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={0, 1, 1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 0}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 0, 1}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 1, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, 2}, .cell={1, 1, 0, 1}}}, .ptr={1, 2, 2}};
TrackPiece trackPieceForQuarterTurn3Tiles25DegDown = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 0, 1}}, 
    {.coord={0, 1, 0}, .cell={0, 0, 1, 0}}, 
    {.coord={0, 1, -1}, .cell={0, 0, 1, 0}}, 
    {.coord={1, 0, 0}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 0, -1}, .cell={0, 1, 0, 0}}, 
    {.coord={1, 1, 0}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, -1}, .cell={1, 1, 0, 1}}, 
    {.coord={1, 1, -2}, .cell={1, 1, 0, 1}}}, .ptr={1, 2, -2}};
TrackPiece trackPieceForQuarterTurn3Tiles60DegUp = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 2}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 3}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 4}, .cell={1, 1, 1, 1}}}, .ptr={0, 1, 4}};
TrackPiece trackPieceForQuarterTurn3Tiles60DegDown = {.shape={
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -2}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -3}, .cell={1, 1, 1, 1}}, 
    {.coord={0, 0, -4}, .cell={1, 1, 1, 1}}}, .ptr={0, 1, -4}};
TrackPiece trackPieceForRightVerticalLoop = {.shape={
    {.coord={0, 0, -1}, .cell={1, 1, 1, 1}},
    {.coord={0, 0, 0}, .cell={1, 1, 1, 1}},
    {.coord={0, 0, 1}, .cell={1, 1, 1, 1}},
    {.coord={1, 0, 0}, .cell={1, 1, 1, 1}},
    {.coord={1, 0, 1}, .cell={1, 1, 1, 1}},
    {.coord={1, 0, 2}, .cell={1, 1, 1, 1}},
    {.coord={1, 0, 7}, .cell={0, 1, 0, 1}},
    {.coord={1, 0, 8}, .cell={0, 1, 0, 1}},
    {.coord={1, 0, 9}, .cell={0, 1, 0, 1}},
    {.coord={2, 0, 1}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 2}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 3}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 4}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 5}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 6}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 7}, .cell={0, 1, 0, 0}},
    {.coord={2, 0, 8}, .cell={0, 1, 0, 0}},
    {.coord={1, 1, -1}, .cell={1, 1, 1, 1}},
    {.coord={1, 1, 0}, .cell={1, 1, 1, 1}},
    {.coord={1, 1, 1}, .cell={1, 1, 1, 1}},
    {.coord={0, 1, 0}, .cell={1, 1, 1, 1}},
    {.coord={0, 1, 1}, .cell={1, 1, 1, 1}},
    {.coord={0, 1, 2}, .cell={1, 1, 1, 1}},
    {.coord={0, 1, 7}, .cell={1, 0, 1, 0}},
    {.coord={0, 1, 8}, .cell={1, 0, 1, 0}},
    {.coord={0, 1, 9}, .cell={1, 0, 1, 0}},
    {.coord={-1, 1, 1}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 2}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 3}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 4}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 5}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 6}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 7}, .cell={0, 0, 1, 0}},
    {.coord={-1, 1, 8}, .cell={0, 0, 1, 0}}}, .ptr={2, 1, -1}};

// Makes copies because mirroring happens later.
std::map<track_type_t, TrackPiece> trackData = {
  {TRACK_ELEM_BEGIN_STATION, trackPieceForFlat},
  {TRACK_ELEM_MIDDLE_STATION, trackPieceForFlat},
  {TRACK_ELEM_END_STATION, trackPieceForFlat},
  {TRACK_ELEM_FLAT, trackPieceForFlat},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANK, trackPieceForFlat},
  {TRACK_ELEM_FLAT_TO_25_DEG_UP, trackPieceForFlatTo25DegUp},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP, trackPieceForFlatTo25DegUp},
  {TRACK_ELEM_FLAT_TO_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_UP,
    trackPieceForFlatTo25DegUp},
  {TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN,
    trackPieceFor25DegDown},
  {TRACK_ELEM_RIGHT_BANK, trackPieceForFlat},
  {TRACK_ELEM_RIGHT_BANK_TO_FLAT, trackPieceForFlat},
  {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP, trackPieceForFlatTo25DegUp},
  {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_25_DEG_UP_RIGHT_BANKED, trackPieceFor25DegUp},
  {TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES,
    trackPieceForQuarterTurn5Tiles},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK, trackPieceForQuarterTurn3Tiles},
  {TRACK_ELEM_25_DEG_UP_TO_FLAT, trackPieceFor25DegUpToFlat},
  {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK, trackPieceFor25DegUpToFlat},
  {TRACK_ELEM_25_DEG_UP, trackPieceFor25DegUp},
  {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP, trackPieceFor25DegUp},
  {TRACK_ELEM_25_DEG_UP_TO_60_DEG_UP, trackPieceFor25DegUpTo60DegUp},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP, trackPieceFor25DegUp},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT,
    trackPieceFor25DegUpToFlat},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT, trackPieceFor25DegUpToFlat},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
    trackPieceForQuarterTurn5Tiles25DegUp},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
    trackPieceForQuarterTurn3Tiles25DegUp},
  {TRACK_ELEM_60_DEG_UP_TO_25_DEG_UP, trackPieceFor25DegUpTo60DegUp},
  {TRACK_ELEM_60_DEG_UP, trackPieceFor60DegUp},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP,
    trackPieceForQuarterTurn3Tiles60DegUp},
  {TRACK_ELEM_25_DEG_DOWN_TO_FLAT, trackPieceFor25DegDownToFloat},
  {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK, trackPieceFor25DegDownToFloat},
  {TRACK_ELEM_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_25_DEG_DOWN_TO_60_DEG_DOWN, trackPieceFor25DegDownTo60DegDown},
  {TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED, trackPieceFor25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN, trackPieceFor25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT,
    trackPieceFor25DegDownToFloat},
  {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT, trackPieceFor25DegDownToFloat},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
    trackPieceForQuarterTurn5Tiles25DegDown},
  {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
    trackPieceForQuarterTurn3Tiles25DegDown},
  {TRACK_ELEM_60_DEG_DOWN_TO_25_DEG_DOWN, trackPieceFor25DegDownTo60DegDown},
  {TRACK_ELEM_60_DEG_DOWN, trackPieceFor60DegDown},
  {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN,
    trackPieceForQuarterTurn3Tiles60DegDown},
  {TRACK_ELEM_RIGHT_VERTICAL_LOOP, trackPieceForRightVerticalLoop},
  // Only used to begin coaster. All other turns are banked.
  {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES, trackPieceForQuarterTurn3Tiles},
};

std::map<std::pair<track_type_t, DirectionType>, TrackPiece> trackDataRot;

bool operator==(const Coord& a, const Coord& b);
Coord AddCoords(const Coord& c0, const Coord& c1);
bool OutOfBounds(const Coord& coord);
Coord MirrorCoord(const Coord& coord);
TrackCell MirrorTrackCell(const TrackCell& tc);
TrackPiece MirrorTrackPiece(const TrackPiece& tp);
Coord RotateCoord(const Coord& coord);
TrackCell RotateTrackCell(const TrackCell& tc);
TrackPiece RotateTrackPiece(const TrackPiece& tp);
Cell ReadSpace(Cell **space, const Coord& ptr);
void WriteSpace(Cell **space, const Coord& ptr, Cell newCells);
void CopySpace(Cell **source, Cell **dest);
std::optional<Cell> ResolveCells(const Cell& c0, const Cell& c1);
bool AddTrackToSpace(
  Cell **space,
  const Coord& ptr,
  DirectionType dir, 
  const TrackDesignTrackElement& track);
bool AddTrackToStack(
  std::vector<GeneratorInfo> *stack,
  const TrackDesignTrackElement& track);
bool ChooseTrack(
  std::vector<GeneratorInfo> *stack,
  std::set<track_type_t>* failedTracks,
  std::vector<track_type_t>* nextPossibleTracks);
std::vector<TrackDesignTrackElement> Generate();

/*
 * Definitions
 */

DirectionType TurnLeft(DirectionType dir) {
  switch (dir) {
    case kNorth:
      return kWest;
    case kWest:
      return kSouth;
    case kSouth:
      return kEast;
    case kEast:
    default:
      break;
  }
  return kNorth;
}

DirectionType TurnRight(DirectionType dir) {
  switch (dir) {
    case kNorth:
      return kEast;
    case kEast:
      return kSouth;
    case kSouth:
      return kWest;
    case kWest:
    default:
      break;
  }
  return kNorth;
}

bool operator==(const Coord& a, const Coord& b) {
  return a.y == b.y && a.x == b.x && a.z == b.z;
}

Coord AddCoords(const Coord& c0, const Coord& c1) {
  return {c0.y + c1.y, c0.x + c1.x, c0.z + c1.z};
}

bool OutOfBounds(const Coord& coord) {
  if (coord.y < 0 || coord.y >= kSizeY) {
    return true;
  }
  if (coord.x < 0 || coord.x >= kSizeX) {
    return true;
  }
  if (coord.z < 0 || coord.z >= kSizeZ) {
    return true;
  }
  return false;
}


Coord MirrorCoord(const Coord& coord) {
  return {coord.y, -coord.x, coord.z};
}

TrackCell MirrorTrackCell(const TrackCell& tc) {
  return {
    .coord = MirrorCoord(tc.coord), 
    .cell = {tc.cell.c01, tc.cell.c00, tc.cell.c11, tc.cell.c10}};
}

TrackPiece MirrorTrackPiece(const TrackPiece& tp) {
  TrackPiece mirroredPiece;
  for (const auto& tc : tp.shape) {
    mirroredPiece.shape.push_back(MirrorTrackCell(tc));
  }
  mirroredPiece.ptr = MirrorCoord(tp.ptr);
  return mirroredPiece;
}

Coord RotateCoord(const Coord& coord) {
  return {-coord.x, coord.y, coord.z};
}

TrackCell RotateTrackCell(const TrackCell& tc) {
  return {
    .coord = RotateCoord(tc.coord),
    .cell = {tc.cell.c01, tc.cell.c11, tc.cell.c00, tc.cell.c10}};
}

TrackPiece RotateTrackPiece(const TrackPiece& tp) {
  TrackPiece rotatedPiece;
  for (const auto& tc : tp.shape) {
    rotatedPiece.shape.push_back(RotateTrackCell(tc));
  }
  rotatedPiece.ptr = RotateCoord(tp.ptr);
  return rotatedPiece;
}

Cell ReadSpace(Cell **space, const Coord& ptr) {
  return (*space)[kSizeX * kSizeY * ptr.z + kSizeX * ptr.y + ptr.x];
}

void WriteSpace(Cell **space, const Coord& ptr, Cell newCells) {
  (*space)[kSizeX * kSizeY * ptr.z + kSizeX * ptr.y + ptr.x] = newCells;
}

void CopySpace(Cell **source, Cell **dest) {
  for (int i = 0; i < kSizeY * kSizeX * kSizeZ; ++i)  {
    (*dest)[i] = (*source)[i];
  }
}

std::optional<Cell> ResolveCells(const Cell& c0, const Cell& c1) {
  if (c0.c00 == 1 && c1.c00 == 1) {
    return std::nullopt;
  }
  if (c0.c01 == 1 && c1.c01 == 1) {
    return std::nullopt;
  }
  if (c0.c10 == 1 && c1.c10 == 1) {
    return std::nullopt;
  }
  if (c0.c11 == 1 && c1.c11 == 1) {
    return std::nullopt;
  }
  return Cell{
    .c00 = c0.c00 | c1.c00,
    .c01 = c0.c01 | c1.c01,
    .c10 = c0.c10 | c1.c10,
    .c11 = c0.c11 | c1.c11,
  };
}

bool AddTrackToSpace(
  Cell **space,
  const Coord& ptr,
  DirectionType dir, 
  const TrackDesignTrackElement& track) {

  const TrackPiece& tp = trackDataRot[{track.type, dir}];
  for (const TrackCell& tc : tp.shape) {
    const Coord& newPtr = AddCoords(ptr, tc.coord);
    if (OutOfBounds(newPtr)) {
      return false;
    }

    const Cell& origCell = ReadSpace(space, newPtr);
    const auto& newCell = ResolveCells(origCell, tc.cell);
    if (!newCell.has_value()) {
      return false;
    }
    WriteSpace(space, newPtr, *newCell);
  }
  return true;
}

bool AddTrackToStack(
  std::vector<GeneratorInfo> *stack,
  const TrackDesignTrackElement& track) {

  GeneratorInfo lastInfo = stack->at(stack->size() - 1);

  // Debug
  // auto p = lastInfo.ptr;
  // std::cout << "At " << p.y << ", " << p.x << ", " << p.z << std::endl;

  const TrackPiece& trackPiece = trackDataRot[{track.type, lastInfo.dir}];
  Coord newPtr = AddCoords(lastInfo.ptr, trackPiece.ptr);
  if (OutOfBounds(newPtr)) {
    return false;
  }

  // Height limiting.
  float fZ = static_cast<float>(newPtr.z);
  float fLimit = static_cast<float>(kSizeZ);
  float fTrackSize = static_cast<float>(lastInfo.tracks.size());
  float limit = fLimit;
  if (lastInfo.tracks.size() > 10) {
    limit = fLimit - fTrackSize * 0.05;
  }
  if (fZ > limit) {
    return false;
  }

  // Copy old tracks.
  std::vector<TrackDesignTrackElement> newTracks = lastInfo.tracks;
  newTracks.push_back(track);

  DirectionType newDir = lastInfo.dir;
  auto it = dirStateMachine.find(track.type);
  if (it != dirStateMachine.end()) {
    newDir = it->second(newDir);
  }

  // Allocate space.
  Cell *newSpace = (Cell*) malloc(sizeof(Cell) * kSizeX * kSizeY * kSizeZ);
  CopySpace(&lastInfo.space, &newSpace);

  if (!AddTrackToSpace(&newSpace, lastInfo.ptr, lastInfo.dir, track)) {
    free(newSpace);
    return false;
  }

  stack->push_back(GeneratorInfo{
    .space = newSpace, 
    .tracks = newTracks,
    .ptr = newPtr,
    .dir = newDir,
    .failedTracks = {}});
  return true;
}

bool ChooseTrack(
  std::vector<GeneratorInfo> *stack,
  std::set<track_type_t> *failedTracks,
  std::vector<track_type_t>* nextPossibleTracks) {

  std::vector<track_type_t> nextPossibleUpdated;
  for (const auto& npt : *nextPossibleTracks) {
    if (failedTracks->find(npt) == failedTracks->end()) {
      nextPossibleUpdated.push_back(npt);
    }
  }
  while (true) {
    if (nextPossibleUpdated.empty()) {
      return false;
    }

    // Hack. If we can, do some looping :)
    int i = -1;
    auto it = std::find(nextPossibleUpdated.begin(), nextPossibleUpdated.end(),
      TRACK_ELEM_RIGHT_VERTICAL_LOOP);
    if (it != nextPossibleUpdated.end()) {
      i = std::distance(nextPossibleUpdated.begin(), it);
    }
    it = std::find(nextPossibleUpdated.begin(), nextPossibleUpdated.end(),
      TRACK_ELEM_LEFT_VERTICAL_LOOP);
    if (it != nextPossibleUpdated.end()) {
      i = std::distance(nextPossibleUpdated.begin(), it);
    }
    if (i == -1) {
      i = rand() % nextPossibleUpdated.size();
    }

    // int i = rand() % nextPossibleUpdated.size();
    const auto& nextTrack = nextPossibleUpdated[i];
    if (AddTrackToStack(stack, {nextTrack, 4})) {
      break;
    }
    failedTracks->insert(nextTrack);
    nextPossibleUpdated.erase(nextPossibleUpdated.begin() + i);
  }
  return true;
}

std::vector<TrackDesignTrackElement> Generate() {
  std::map<track_type_t, track_type_t> mirrorMap = {
    {TRACK_ELEM_FLAT_TO_LEFT_BANK, TRACK_ELEM_FLAT_TO_RIGHT_BANK},
    {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP,
      TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP},
    {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
      TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN},
    {TRACK_ELEM_LEFT_BANK, TRACK_ELEM_RIGHT_BANK},
    {TRACK_ELEM_LEFT_BANK_TO_FLAT, TRACK_ELEM_RIGHT_BANK_TO_FLAT},
    {TRACK_ELEM_LEFT_BANK_TO_25_DEG_UP, TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP},
    {TRACK_ELEM_LEFT_BANK_TO_25_DEG_DOWN, TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN},
    {TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_UP,
      TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_UP},
    {TRACK_ELEM_LEFT_BANKED_FLAT_TO_LEFT_BANKED_25_DEG_DOWN,
      TRACK_ELEM_RIGHT_BANKED_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN},
    {TRACK_ELEM_25_DEG_UP_LEFT_BANKED, TRACK_ELEM_25_DEG_UP_RIGHT_BANKED},
    {TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES,
      TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES},
    {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK,
      TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK},
    {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANK, TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK},
    {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANKED_25_DEG_UP,
      TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_25_DEG_UP,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_LEFT_BANKED_FLAT,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_FLAT,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP,
      TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP,
      TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP},
    {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP,
      TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP},
    {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANK, TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK},
    {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANKED_25_DEG_DOWN,
      TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN},
    {TRACK_ELEM_25_DEG_DOWN_LEFT_BANKED, TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_LEFT_BANKED_FLAT,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_FLAT,
      TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN,
      TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN,
      TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN},
    {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN,
      TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN},
    {TRACK_ELEM_LEFT_VERTICAL_LOOP, TRACK_ELEM_RIGHT_VERTICAL_LOOP},
    {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES,
      TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES},
  };

  // Generate mirrored data.
  for (auto [left, right]: mirrorMap) {
    trackData[left] = MirrorTrackPiece(trackData[right]);
  }

  // Generate rotated data.
  for (auto [trackType, trackPiece]: trackData) {
    trackDataRot[{trackType, kNorth}] = trackPiece;
    TrackPiece curTrack = trackPiece;
    for (DirectionType dir : {kEast, kSouth, kWest}) {
      curTrack = RotateTrackPiece(curTrack);
      trackDataRot[{trackType, dir}] = curTrack;
    }
  }

  int attempt = 0;
  while (true) {
    // srand(attempt);
    std::cout << "Generating, attempt " << attempt++ << "..." << std::endl;

    // Allocate space.
    Cell *space = (Cell*) malloc(sizeof(Cell) * kSizeX * kSizeY * kSizeZ);
    for (int i = 0; i < kSizeY * kSizeX * kSizeZ; ++i) {
      space[i] = {0, 0, 0, 0};
    }

    // Reserve space for entrance/exit.
    /*
    for (int z = 0; z < 4; ++z) {
      for (int y = 5; y < 7; ++y) {
        for (int x = 10; x < 12; ++x) {
          WriteSpace(&space, {y, x, z}, {1, 1, 1, 1});
        }
      }
    }
    */

    // Reserve tile before station begin.
    Coord endCoord = {0, 3, 0};
    WriteSpace(&space, endCoord, {1, 1, 1, 1});
    WriteSpace(&space, {0, 3, 1}, {1, 1, 1, 1});

    std::vector<GeneratorInfo> stack;
    stack.push_back(GeneratorInfo{
      .space = space, 
      .tracks = {},
      .ptr = {0, 4, 0},
      .dir = kEast,
      .failedTracks = {}});

    // Generate initial track.
    std::vector<TrackDesignTrackElement> tracksToAdd;
    tracksToAdd.push_back({TRACK_ELEM_BEGIN_STATION, 4});
    for (int i = 0; i < 2; ++i) {
      tracksToAdd.push_back({TRACK_ELEM_MIDDLE_STATION, 4});
    }
    tracksToAdd.push_back({TRACK_ELEM_END_STATION, 4});
    tracksToAdd.push_back({TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP, 4});
    tracksToAdd.push_back(
      {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, 4});
    tracksToAdd.push_back(
      {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, 4});

    for (const auto& track : tracksToAdd) {
      if (!AddTrackToStack(&stack, track)) {
        std::cout << "Failed to add " << track.type << std::endl;
        return {};
      }
    }

    int steps = 0;
    bool success = false;
    while (true) {
      GeneratorInfo *lastInfo = &(stack[stack.size() - 1]);

      // Check end condition.
      if (lastInfo->ptr == endCoord && lastInfo->dir == kEast) {
        success = lastInfo->tracks.size() > kMinimumTrackSize;
        // Has to contain at least one loop.
        /*
        success &= std::find_if(lastInfo->tracks.begin(),
                                lastInfo->tracks.end(), [](const auto& track) {
            return track.type == TRACK_ELEM_LEFT_VERTICAL_LOOP 
              || track.type == TRACK_ELEM_RIGHT_VERTICAL_LOOP; })
              != lastInfo->tracks.end();
        */
        break;
      }

      auto lastTrack = lastInfo->tracks[lastInfo->tracks.size() - 1];
      auto* nextPossibleTracks = trackStateMachine[lastTrack.type];

      // Debug(&stack);
      if (ChooseTrack(&stack, &(lastInfo->failedTracks), nextPossibleTracks)) {
        continue;
      }

      // Backtrack.
      free(lastInfo->space);
      stack.pop_back();

      lastInfo = &(stack[stack.size() - 1]);
      lastInfo->failedTracks.insert(lastTrack.type);

      steps++;
      if (steps > kTryPerAttempt) {
        break;
      }
    }

    auto tracks = stack[stack.size() - 1].tracks;
    while (!stack.empty()) {
      free(stack[stack.size() - 1].space);
      stack.pop_back();
    }

    if (success) {
      return tracks;
    }
  }
}

/*
 * Main
 */

int main(int argc, const char** argv)
{
  srand(time(NULL));

  auto importer = TrackImporter::CreateTD6();
  if (!importer->Load(kTrackToLoad)) {
    std::cout << "Load failed" << std::endl;
    return -1;
  }
  auto td = importer->Import();
  td->track_elements.clear();
  td->entrance_elements.clear();
  
  const auto& tracks = Generate();

  /*
  // Debugging
  std::map<track_type_t, std::string> debugMap = {
    {TRACK_ELEM_FLAT, "TRACK_ELEM_FLAT"},
    {TRACK_ELEM_FLAT_TO_LEFT_BANK, "TRACK_ELEM_FLAT_TO_LEFT_BANK"},
    {TRACK_ELEM_FLAT_TO_RIGHT_BANK, "TRACK_ELEM_FLAT_TO_RIGHT_BANK"},
    {TRACK_ELEM_FLAT_TO_25_DEG_UP, "TRACK_ELEM_FLAT_TO_25_DEG_UP"},
    {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP, "TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_UP"},
    {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP, "TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_UP"},
    {TRACK_ELEM_FLAT_TO_25_DEG_DOWN, "TRACK_ELEM_FLAT_TO_25_DEG_DOWN"},
    {TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_DOWN, "TRACK_ELEM_FLAT_TO_LEFT_BANKED_25_DEG_DOWN"},
    {TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN, "TRACK_ELEM_FLAT_TO_RIGHT_BANKED_25_DEG_DOWN"},
    {TRACK_ELEM_LEFT_BANK, "TRACK_ELEM_LEFT_BANK"},
    {TRACK_ELEM_LEFT_BANK_TO_FLAT, "TRACK_ELEM_LEFT_BANK_TO_FLAT"},
    {TRACK_ELEM_LEFT_BANK_TO_25_DEG_UP, "TRACK_ELEM_LEFT_BANK_TO_25_DEG_UP"},
    {TRACK_ELEM_LEFT_BANK_TO_25_DEG_DOWN, "TRACK_ELEM_LEFT_BANK_TO_25_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_UP_LEFT_BANKED, "TRACK_ELEM_25_DEG_UP_LEFT_BANKED"},
    {TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES, "TRACK_ELEM_BANKED_LEFT_QUARTER_TURN_5_TILES"},
    {TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK, "TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES_BANK"},
    {TRACK_ELEM_RIGHT_BANK, "TRACK_ELEM_RIGHT_BANK"},
    {TRACK_ELEM_RIGHT_BANK_TO_FLAT, "TRACK_ELEM_RIGHT_BANK_TO_FLAT"},
    {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP, "TRACK_ELEM_RIGHT_BANK_TO_25_DEG_UP"},
    {TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN, "TRACK_ELEM_RIGHT_BANK_TO_25_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_UP_RIGHT_BANKED, "TRACK_ELEM_25_DEG_UP_RIGHT_BANKED"},
    {TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES, "TRACK_ELEM_BANKED_RIGHT_QUARTER_TURN_5_TILES"},
    {TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK, "TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES_BANK"},
    {TRACK_ELEM_25_DEG_UP_TO_FLAT, "TRACK_ELEM_25_DEG_UP_TO_FLAT"},
    {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANK, "TRACK_ELEM_25_DEG_UP_TO_LEFT_BANK"},
    {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK, "TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANK"},
    {TRACK_ELEM_25_DEG_UP, "TRACK_ELEM_25_DEG_UP"},
    {TRACK_ELEM_25_DEG_UP_TO_LEFT_BANKED_25_DEG_UP, "TRACK_ELEM_25_DEG_UP_TO_LEFT_BANKED_25_DEG_UP"},
    {TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP, "TRACK_ELEM_25_DEG_UP_TO_RIGHT_BANKED_25_DEG_UP"},
    {TRACK_ELEM_25_DEG_UP_TO_60_DEG_UP, "TRACK_ELEM_25_DEG_UP_TO_60_DEG_UP"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_25_DEG_UP, "TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_25_DEG_UP"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_LEFT_BANKED_FLAT, "TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_LEFT_BANKED_FLAT"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_FLAT, "TRACK_ELEM_LEFT_BANKED_25_DEG_UP_TO_FLAT"},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, "TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP"},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP, "TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP, "TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_25_DEG_UP"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT, "TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_RIGHT_BANKED_FLAT"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT, "TRACK_ELEM_RIGHT_BANKED_25_DEG_UP_TO_FLAT"},
    {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP, "TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_UP"},
    {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP, "TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_UP"},
    {TRACK_ELEM_60_DEG_UP_TO_25_DEG_UP, "TRACK_ELEM_60_DEG_UP_TO_25_DEG_UP"},
    {TRACK_ELEM_60_DEG_UP, "TRACK_ELEM_60_DEG_UP"},
    {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP, "TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_UP"},
    {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP, "TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_UP"},
    {TRACK_ELEM_25_DEG_DOWN_TO_FLAT, "TRACK_ELEM_25_DEG_DOWN_TO_FLAT"},
    {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANK, "TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANK"},
    {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK, "TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANK"},
    {TRACK_ELEM_25_DEG_DOWN, "TRACK_ELEM_25_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANKED_25_DEG_DOWN, "TRACK_ELEM_25_DEG_DOWN_TO_LEFT_BANKED_25_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN, "TRACK_ELEM_25_DEG_DOWN_TO_RIGHT_BANKED_25_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_DOWN_TO_60_DEG_DOWN, "TRACK_ELEM_25_DEG_DOWN_TO_60_DEG_DOWN"},
    {TRACK_ELEM_25_DEG_DOWN_LEFT_BANKED, "TRACK_ELEM_25_DEG_DOWN_LEFT_BANKED"},
    {TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED, "TRACK_ELEM_25_DEG_DOWN_RIGHT_BANKED"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN, "TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_LEFT_BANKED_FLAT, "TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_LEFT_BANKED_FLAT"},
    {TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_FLAT, "TRACK_ELEM_LEFT_BANKED_25_DEG_DOWN_TO_FLAT"},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN, "TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN"},
    {TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN, "TRACK_ELEM_LEFT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN, "TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_25_DEG_DOWN"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT, "TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_RIGHT_BANKED_FLAT"},
    {TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT, "TRACK_ELEM_RIGHT_BANKED_25_DEG_DOWN_TO_FLAT"},
    {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN, "TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_5_TILE_25_DEG_DOWN"},
    {TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN, "TRACK_ELEM_RIGHT_BANKED_QUARTER_TURN_3_TILE_25_DEG_DOWN"},
    {TRACK_ELEM_60_DEG_DOWN_TO_25_DEG_DOWN, "TRACK_ELEM_60_DEG_DOWN_TO_25_DEG_DOWN"},
    {TRACK_ELEM_60_DEG_DOWN, "TRACK_ELEM_60_DEG_DOWN"},
    {TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN, "TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE_60_DEG_DOWN"},
    {TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN, "TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE_60_DEG_DOWN"},
    {TRACK_ELEM_LEFT_VERTICAL_LOOP, "TRACK_ELEM_LEFT_VERTICAL_LOOP"},
    {TRACK_ELEM_RIGHT_VERTICAL_LOOP, "TRACK_ELEM_RIGHT_VERTICAL_LOOP"},
  };

  for (size_t i = 0; i < tracks.size(); ++i) {
    std::cout << i << ": " << debugMap[tracks[i].type] << std::endl;
  }

  std::vector<TrackDesignTrackElement> debugTracks;
  for (size_t i = 0; i < 16; ++i) {
    debugTracks.push_back(tracks[i]);
  }
  td->track_elements = debugTracks;
  */

  td->track_elements = tracks;

  T6Exporter exporter(td.get());
  if (!exporter.SaveTrack(kTrackToSave)) {
    std::cout << "Failed saving track" << std::endl;
  }

  std::cout << "Ok: " << tracks.size() << std::endl;
  return 0;
}
