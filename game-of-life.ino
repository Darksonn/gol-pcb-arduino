String inputString = ""; // The message we receive via bluetooth

char configuration[27]; // Stores the configuration of the board.
int mapping[5][5] = { // maps a coordinate to an LED
  {26, 25, 24, 23, 22},
  {1, 0, 21, 20, 19},
  {3, 2, 18, 17, 15},
  {5, 4, 14, 12, 13},
  {7, 6, 11, 10, 9}
};
// sets everthing to 0
inline void clear_config() {
  int i;
  for (i = 0; i < 27; ++i) configuration[i] = 0;
}
// sets the LED at the given coordinate to the given state
// does not update the actual LEDs
inline void set(int x, int y, char value) {
  int m = mapping[x][y];
  configuration[m] = value;
}

// send a clock pulse to the shift registers
inline void clockRegister(int regId){
  digitalWrite(regId, HIGH);
  digitalWrite(regId, LOW);
}
// sends the board configuration over bluetooth to the phone
inline void send_board() {
  int i;
  for (i = 0; i < 25; ++i) Serial.print(configuration[mapping[i/5][i%5]] == LOW ? 'L' : 'H');
  Serial.println();
}
// stores the configuration currently stored in the shift registers
char written_config[27];
// updates the shift registers to the new contents
void write_config() {
  int i;
  // check if the state has changed, to avoid blinking the LEDs when we update the shift registers
  char same = true;
  for (i = 0; i < 27; ++i) {
    if (written_config[i] != configuration[i]) same = false;
  }
  // if we haven't changed, abort:
  if (same) return;
  int sum = 0;
  // store the new configuration in written_config
  for (i = 0; i < 27; ++i) {
    written_config[i] = configuration[i];
    sum += configuration[i];
  }
  // 3 of the LEDs are controlled directly with a digital pin
  digitalWrite(6, configuration[24]);
  digitalWrite(7, configuration[25]);
  digitalWrite(8, configuration[26]);
  // if the sum is zero, the new board is empty, so we can clear instead of sending a lot of zeroes
  if (sum == 0) {
    digitalWrite(5, LOW);
    digitalWrite(5, HIGH);
    return;
  }
  // send the pins to the shift registers
  clockRegister(4);
  for (i = 0; i < 24; ++i) {
    digitalWrite(2, configuration[i]);
    clockRegister(4);
  }
}

// these variables are used in computing the next game of life step
// gol is short for game of life
char gol[5][5];
// contains changes to the board, that have not yet been committed to the board
// the value 2 is stored if nothing has been changed at the given coordinate
// we are storing them here, to avoid changing the gol array while the arduino is computing the next step
int changes[5][5];

void setup() {
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  // this is DATA A on the first shift register, there is no need to ever turn it off
  digitalWrite(3,HIGH);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(4, HIGH);
  // Clear the shift registers
  digitalWrite(5, LOW);
  digitalWrite(5, HIGH);
  Serial.begin(9600);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  // Clear the remaining 3 LEDs
  digitalWrite(6, LOW);
  digitalWrite(7, LOW);
  digitalWrite(8, LOW);
  int i;
  // clear the game of life data
  for (i = 0; i < 27; ++i) gol[i%5][i/5] = 0;
  // nothing has been changed yet, so clear changes
  for (i = 0; i < 27; ++i) changes[i%5][i/5] = 2;
}

// reads from the gol array, but wraps around in case the arguments are out of bound
inline char golr(int x, int y) {
  while (x < 0) x += 5;
  while (y < 0) y += 5;
  x = x % 5;
  y = y % 5;
  return gol[x][y];
}

// computes the next state of cell
inline char step(int x, int y) {
  // compute how many live cells are around the current cell
  int lives =
    golr(x-1,y-1)
  + golr(x-1,y)
  + golr(x-1,y+1)
  + golr(x,y-1)
  + golr(x,y+1)
  + golr(x+1,y-1)
  + golr(x+1,y)
  + golr(x+1,y+1);
  switch (lives) {
    // if there are 2 around the cell, it stays the same
    case 2:
      return gol[x][y];
    // if there are 3 around the cell, it is always alive
    case 3:
      return 1;
    // in all other cases, the cell is dead
    default:
      return 0;
  }
}
// computes the next board state under game of life
void gol_step() {
  char to[25];
  int i;
  // store the new state in the to array
  for (i = 0; i < 25; ++i) to[i] = step(i % 5, i / 5);
  // copy the to array to the gol array
  for (i = 0; i < 25; ++i) {
    gol[i % 5][i / 5] = to[i];
  }
  // we need to use another array, since the step function is reading the gol array while we are computing the new state
  send_board();
}

// is game of life paused?
char paused = true;

void loop() {
  int i;
  // if we are not paused, compute the next game of life step
  if (!paused) {
    delay(750);
    gol_step();
  }
  // move changes from the changes array to the gol array
  for (i = 0; i < 25; ++i) {
    int x = i % 5;
    int y = i / 5;
    if (changes[x][y] != 2) {
      gol[x][y] = changes[x][y];
      changes[x][y] = 2;
    }
  }
  // write the board state to the LEDs
  for (i = 0; i < 25; ++i) {
    set(i % 5, i / 5, gol[i%5][i/5] == 1 ? HIGH : LOW);
  }
  write_config();
}

// run when a message has been received over bluetooth
void handleSerialData() {
  if (inputString.length() < 2) return;
  if (inputString.charAt(0) == ' ') return;
  // if we receive the string clr!
  // then clear the board
  if (inputString.charAt(0) == 'c') {
    int i;
    for (i = 0; i < 25; ++i) changes[i%5][i/5] = 0;
    return;
  }
  // when the game is paused or unpaused, we receive "g 0!" or "g 1!"
  if (inputString.charAt(0) == 'g') {
    paused = inputString.charAt(2) == '0';
    return;
  }
  // otherwise we've received change to an LED
  int ledName = inputString.substring(0,inputString.indexOf(" ")).toInt();
  int x = ledName/5;
  int y = ledName%5;
  char state = inputString.charAt(inputString.length()-2);
  changes[x][y] = state == '0' ? 0 : 1;
}

// serialEvent is called whenever we receive data through bluetooth
void serialEvent() {
  // Read all pending data
  while (Serial.available()) {
    // Get the char and add it to our inputString
    char inChar = (char)Serial.read();
    if (inChar == '\n') continue; // ignore newlines
    // add it to the inputString:
    inputString += inChar;
    // If the incoming character is a '!', we have received a full message
    // process the message before reading more input
    if (inChar == '!') {
      handleSerialData();
      inputString = "";
    }
  }
}
