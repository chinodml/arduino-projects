\
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <IRremote.hpp>


// ===== COMPONENT PINS =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int IRpin = 2;
const int buttonPin = 3;

// ===== BUTTON HEX CODE VALUES =====
const byte BTN_1 = 0x45;
const byte BTN_2 = 0x46;
const byte BTN_3 = 0x47;
const byte BTN_4 = 0x44;
const byte BTN_5 = 0x40;
const byte BTN_6 = 0x43; 
const byte BTN_7 = 0x07;
const byte BTN_8 = 0x15;
const byte BTN_9 = 0x09;
const byte BTN_0 = 0x19;
const byte BTN_STAR = 0x16;
const byte BTN_HASH = 0x0D;
const byte BTN_UP = 0x18;
const byte BTN_DOWN = 0x52;
const byte BTN_LEFT = 0x08;
const byte BTN_RIGHT = 0x5A;
const byte BTN_OK = 0x1C;

// ===== T9-STYLE ALPHABET INPUT =====
const char* buttonChars[] = {
  "0",     // BTN_0
  "ABC",   // BTN_1
  "DEF",   // BTN_2
  "GHI",   // BTN_3
  "JKL",   // BTN_4
  "MNO",   // BTN_5
  "PQRS",  // BTN_6
  "TUV",   // BTN_7
  " ",     // BTN_8
  "WXYZ",  // BTN_9
};

// ===== T9 KEYPAD HELPER: BUTTON ASSIGNMENT =====
int getButtonIndex(byte cmd) {
  if (cmd == BTN_1) return 1;
  if (cmd == BTN_2) return 2;
  if (cmd == BTN_3) return 3;
  if (cmd == BTN_4) return 4;
  if (cmd == BTN_5) return 5;
  if (cmd == BTN_6) return 6;
  if (cmd == BTN_7) return 7;
  if (cmd == BTN_8) return 9;
  if (cmd == BTN_9) return 8;
  if (cmd == BTN_0) return 0;
  return -1;  // invalid
}

// ===== PESO SIGN CHARACTER =====
byte pesoChar[8] = {
  // 1 = shaded pixel
  // 0 = unshaded pixel
  B01110,  //  ***
  B01001,  //  *  *
  B11111,  // *****
  B01001,  //  *  *
  B11111,  // *****
  B01110,  //  ***
  B01000,  //  *
  B01000   //  *
};

// ===== OPTION SELECTION =====
String itemOption[4] = {
  "Display",
  "Edit",
  "Add",
  "Delete"
};

// ===== FUNCTION SWITCHES =====
enum AppMode {
  MODE_SELECT,   // contains added items (cannot be removed: repository of items)
  MODE_ADD,      // add items then store it in MODE_SELECT
  MODE_DISPLAY,  // display items accessed from MODE_SELECT
  MODE_EDIT,     // retrieve and update items stored in MODE_SELECT
  MODE_DELETE    // delete items selected from the MODE_SELECT
};

AppMode currentMode = MODE_SELECT;


// ===== ADDING ITEMS =====
#define MAX_ITEMS 10

struct Item {
  String name;
  String price;
};

Item items[MAX_ITEMS];
int itemCount = 0;
int optionCount = 0;

bool isAddingName = true;
bool itemAdded = false;


// ===== STATE VARIABLES =====
unsigned long lastPressTime = 0;          // used to compare with pressTimeout to either change or finalize letter input from T9 keypad
const unsigned long pressTimeout = 1000;  // maximum duration for finalizing input from handleLetterInput()

byte lastButton = 0;
int currentCharIndex = 0;
String typedText = "";  // To store the typed name string
String typedNum = "";   // To store the typed price string
String lastPrintedLine = "";

bool isSelectingItem = false;
bool itemConfirmed = false;
int selectedIndex = 0;

bool systemActive = false;  // Start in OFF state
int buttonState = HIGH;
int lastButtonState = HIGH;


void setup() {
  lcd.begin();
  lcd.noBacklight();
  lcd.setCursor(0, 0);
  lcd.createChar(0, pesoChar);  // Register custom ₱ symbol
  pinMode(buttonPin, INPUT);
  IrReceiver.begin(IRpin, ENABLE_LED_FEEDBACK);
  Serial.begin(9600);
}


void loop() {
  buttonState = digitalRead(buttonPin);

  if (buttonState == LOW && lastButtonState == HIGH) {
    systemActive = !systemActive;

    if (systemActive) {
      // TURN ON / INITIALIZE
      lcd.begin();          // reinitialize LCD
      lcd.backlight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("System Starting");
      delay(1000);
      lcd.clear();
    } else {
      // TURN OFF / SHUTDOWN
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Shutting Down");
      delay(1000);
      lcd.clear();
      lcd.noBacklight();  // turn off display
    }

    delay(300);  // debounce
  }

  lastButtonState = buttonState;

  if (!systemActive) return;  // system is off, do nothing

  // Put your main mode logic here
  switch (currentMode) {
    case MODE_SELECT: selectItem(); break;
    case MODE_ADD: addItem(); break;
    case MODE_DISPLAY: displayItem(); break;
    case MODE_EDIT: editItem(); break;
    case MODE_DELETE: deleteItem(selectedIndex); break;
  }
}





// ===== CORE FUNCTION: T9 LETTER INPUT =====
void handleLetterInput() {
  if (IrReceiver.decode()) {
    byte cmd = IrReceiver.decodedIRData.command;
    unsigned long now = millis();

    // hashtag button for deleting characters
    if (cmd == BTN_HASH && typedText.length() > 0) {
      typedText.remove(typedText.length() - 1);
      lastButton = 0;
      currentCharIndex = 0;
      lastPressTime = now;

    } else {
      int btnIndex = getButtonIndex(cmd);


      if (btnIndex >= 0) {
        // alphabet character change per button press
        if (cmd == lastButton && (now - lastPressTime) < pressTimeout) {
          currentCharIndex++;
          if (currentCharIndex >= strlen(buttonChars[btnIndex])) {
            currentCharIndex = 0;
          }
        } else {
          // committing previous char
          if (lastButton > 0) {
            int lastIndex = getButtonIndex(lastButton);
            if (lastIndex >= 0) {
              typedText += buttonChars[lastIndex][currentCharIndex];
            }
          }

          lastButton = cmd;
          currentCharIndex = 0;
        }

        lastPressTime = now;
      }
    }

    delay(200);
    IrReceiver.resume();
  }

  // Finalize character on timeout
  if (lastButton > 0 && (millis() - lastPressTime) > pressTimeout) {
    int lastIndex = getButtonIndex(lastButton);
    if (lastIndex >= 0) {
      typedText += buttonChars[lastIndex][currentCharIndex];
    }
    lastButton = 0;
    currentCharIndex = 0;
  }

  // updating the characters from the lcd screen
  String previewText = typedText;
  if (lastButton > 0) {
    int btnIndex = getButtonIndex(lastButton);
    if (btnIndex >= 0) {
      previewText += buttonChars[btnIndex][currentCharIndex];
    }
  }


  // displaying the preview on the lcd screen
  if (previewText != lastPrintedLine) {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.blink();
    lcd.print(previewText);
    lastPrintedLine = previewText;
  }
}

// ===== CORE FUNCTION: NUMBER INPUT =====
void handleNumberInput() {
  if (IrReceiver.decode()) {
    byte cmd = IrReceiver.decodedIRData.command;

    // Append numbers or decimal point
    if (typedNum.length() < 8 && cmd != BTN_HASH) {
      if (typedNum.length() < 1) {
        // Show ₱ symbol before number
        lcd.setCursor(0, 1);
        lcd.write(byte(0));
      }

      switch (cmd) {
        case BTN_1: typedNum += "1"; break;
        case BTN_2: typedNum += "2"; break;
        case BTN_3: typedNum += "3"; break;
        case BTN_4: typedNum += "4"; break;
        case BTN_5: typedNum += "5"; break;
        case BTN_6: typedNum += "6"; break;
        case BTN_7: typedNum += "7"; break;
        case BTN_8: typedNum += "8"; break;
        case BTN_9: typedNum += "9"; break;
        case BTN_0: typedNum += "0"; break;
        case BTN_STAR:
          if (!typedNum.endsWith(".")) typedNum += ".";  // Only one decimal point
          break;
      }
    }

    // hashtag button for deleting characters
    if (cmd == BTN_HASH && typedNum.length() > 0) {
      typedNum.remove(typedNum.length() - 1);
    }

    // displaying price preview on the lcd screen with peso sign
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.write(byte(0));  // Print peso sign
    lcd.blink();
    lcd.print(typedNum);

    delay(200);
    IrReceiver.resume();  // Ready for next input
  }
}

void selectItem() {
  itemConfirmed = false;
  isSelectingItem = true;

  // If no items, show message first
  if (itemCount == 0) {
    lcd.clear();
    lcd.print("No items yet");

    unsigned long startTime = millis();
    while (millis() - startTime < 3000) {
      if (IrReceiver.decode()) {
        byte cmd = IrReceiver.decodedIRData.command;
        IrReceiver.resume();

        if (cmd == BTN_OK) {
          showItemOptions();
          return;
        }
      }
    }

    isSelectingItem = false;
    return;
  }

  if (IrReceiver.decode()) {
    byte cmd = IrReceiver.decodedIRData.command;

    if (!itemConfirmed) {
      if (cmd == BTN_RIGHT) {
        selectedIndex = (selectedIndex + 1) % itemCount;
      } else if (cmd == BTN_LEFT) {
        selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
      } else if (cmd == BTN_OK) {
        itemConfirmed = true;
        showItemOptions();
      }

      // Display item:
      lcd.clear();
      // product name at the top;
      lcd.setCursor(0, 0);
      lcd.print(items[selectedIndex].name);
      // product price on the bottom
      lcd.setCursor(0, 1);
      lcd.write(byte(0));
      lcd.print(items[selectedIndex].price);
    }

    delay(200);
    IrReceiver.resume();
  }
}

void showItemOptions() {
  const int numOptions = 4;
  int currentOption = 0;
  bool inOptionMenu = true;

  lcd.clear();

  // prevent auto-selecting ang multiple trigger from the button
  IrReceiver.resume();
  delay(300);

  // selector indication on top to select from the option
  while (inOptionMenu) {
    lcd.clear();
    String top = "> " + itemOption[currentOption];
    lcd.setCursor(0, 0);
    lcd.print(top);

    String bottom = itemOption[(currentOption + 1) % numOptions];
    lcd.setCursor(2, 1);
    lcd.print(bottom);

    // reset the IR receiver to accept another input (preferrably BTN_OK)
    IrReceiver.resume();
    delay(300);

    while (!IrReceiver.decode())
      ;
    byte cmd = IrReceiver.decodedIRData.command;

    // scroll through item options using BTN_UP and BTN_DOWN;
    if (cmd == BTN_DOWN) {
      currentOption = (currentOption + 1) % numOptions;
    } else if (cmd == BTN_UP) {
      currentOption = (currentOption - 1 + numOptions) % numOptions;
      // press BTN_OK to select action
    } else if (cmd == BTN_OK) {
      lcd.clear();
      String selected = "* " + itemOption[currentOption] + "       ";
      lcd.setCursor(0, 0);
      lcd.print(selected);
      delay(1000);

      inOptionMenu = false;  // release from option selection
      itemConfirmed = false;


      // toggle case switch to choose action
      switch (currentOption) {
        case 0: currentMode = MODE_DISPLAY; break;
        case 1: currentMode = MODE_EDIT; break;
        case 2: currentMode = MODE_ADD; break;
        case 3: currentMode = MODE_DELETE; break;
      }
      return;
    }
    delay(200);
    IrReceiver.resume();
  }
  itemConfirmed = false;
}

void addItem() {
  static bool hasCleared = false;

  // reset clearing status to clear screen next time accessing MODE_ADD again
  if (currentMode != MODE_ADD) {
    hasCleared = false;
    return;
  }

  // prevent continuous clearing of screen which prevent user input
  if (!hasCleared) {
    lcd.clear();
    hasCleared = true;
  }

  // prevent adding item if maximum number has been met
  if (itemCount >= MAX_ITEMS) {
    lcd.clear();
    lcd.print("List Full!");
    delay(1000);
    currentMode = MODE_SELECT;  // Go back if full
    hasCleared = false;         // Reset for next add cycle
    return;
  }

  // Input Phase
  if (isAddingName) {
    handleLetterInput();

    if (IrReceiver.decode()) {
      byte cmd = IrReceiver.decodedIRData.command;

      if (cmd == BTN_OK) {
        isAddingName = false;
        lastPrintedLine = "";  // Force clear for new line
        lcd.clear();
        lcd.noBlink();
        lcd.setCursor(0, 0);
        lcd.print(typedText);
        lcd.setCursor(0, 1);
        lcd.write(byte(0));  // Print ₱
        lcd.print(typedNum);
        delay(300);
        IrReceiver.resume();
      }
    }
  } else {
    handleNumberInput();

    if (IrReceiver.decode()) {
      byte cmd = IrReceiver.decodedIRData.command;

      if (cmd == BTN_OK) {
        items[itemCount].name = typedText;
        items[itemCount].price = typedNum;
        itemCount++;

        lcd.clear();
        lcd.noBlink();
        lcd.print("Item Added:");
        lcd.setCursor(0, 1);
        lcd.print(typedText);
        lcd.print(" ");
        lcd.write(byte(0));
        lcd.print(typedNum);
        delay(1000);

        // Reset all states for next round
        currentMode = MODE_SELECT;
        typedText = "";
        typedNum = "";
        lastPrintedLine = "";
        lastButton = 0;
        currentCharIndex = 0;
        isAddingName = true;
        itemAdded = true;
        hasCleared = false;  //
        IrReceiver.resume();
      }
    }
  }
}

// display selected item and return to item selection (selectItem function contain added items)
void displayItem() {
  if (IrReceiver.decode()) {
    byte cmd = IrReceiver.decodedIRData.command;
    delay(200);
    if (cmd == BTN_RIGHT) {
      selectedIndex = (selectedIndex + 1) % itemCount;
    } else if (cmd == BTN_LEFT) {
      selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
    } else if (cmd == BTN_OK) {
      itemConfirmed = false;
      currentMode = MODE_SELECT;  // selecting item and returning to MDOE_SELECT at the same time
      IrReceiver.resume();
      delay(300);
      return;
    }

    // displaying selected item
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(items[selectedIndex].name);
    lcd.setCursor(0, 1);
    lcd.write(byte(0));
    lcd.print(items[selectedIndex].price);

    IrReceiver.resume();
    delay(200);
  }
}

void editItem() {
  static int fieldIndex = 0;  // 0 = name, 1 = price
  static bool selectingField = true;
  static bool initialized = false;

  if (!initialized) {
    lcd.clear();
    typedText = items[selectedIndex].name;
    typedNum = items[selectedIndex].price;
    lastPrintedLine = "";
    selectingField = true;
    fieldIndex = 0;
    initialized = true;
  }

  if (selectingField) {
    // Show item and selection arrow
    lcd.setCursor(0, 0);
    lcd.print((fieldIndex == 0) ? "> " : "  ");
    lcd.print(items[selectedIndex].name);
    lcd.print("       ");  // padding clear

    lcd.setCursor(0, 1);
    lcd.print((fieldIndex == 1) ? "> " : "  ");
    lcd.write(byte(0));
    lcd.print(items[selectedIndex].price);
    lcd.print("       ");

    if (IrReceiver.decode()) {
      byte cmd = IrReceiver.decodedIRData.command;

      if (cmd == BTN_DOWN || cmd == BTN_UP) {
        delay(200);
        fieldIndex = 1 - fieldIndex;
      } else if (cmd == BTN_OK) {
        selectingField = false;
        lcd.clear();
        lastPrintedLine = "";
      }

      IrReceiver.resume();
      delay(200);
    }
  } else {
    if (fieldIndex == 0) {
      handleLetterInput();
    } else {
      handleNumberInput();
    }

    if (IrReceiver.decode()) {
      byte cmd = IrReceiver.decodedIRData.command;
      if (cmd == BTN_OK) {
        // Commit the edit
        if (fieldIndex == 0)
          items[selectedIndex].name = typedText;
        else
          items[selectedIndex].price = typedNum;

        // Reset and return
        typedText = "";
        typedNum = "";
        lastPrintedLine = "";
        lastButton = 0;
        currentCharIndex = 0;
        isAddingName = true;
        initialized = false;
        itemConfirmed = false;  // ← tells it to go back to item selection, not option menu
        lcd.noBlink();
        currentMode = MODE_SELECT;
      }
    }
  }
}

void deleteItem(int index) {
  const char* confirmOptions[] = { "Confirm", "Cancel" };
  int confirmIndex = 0;
  bool confirmed = false;

  lcd.clear();
  IrReceiver.resume();
  delay(300);  // Prevent key bounce from previous OK

  while (!confirmed) {
    lcd.clear();
    for (int i = 0; i < 2; i++) {
      lcd.setCursor(0, i);
      lcd.print((i == confirmIndex) ? "> " : "  ");
      lcd.print(confirmOptions[i]);
    }

    while (!IrReceiver.decode())
      ;  // Wait for input

    byte cmd = IrReceiver.decodedIRData.command;

    if (cmd == BTN_UP || cmd == BTN_DOWN) {
      delay(200);
      confirmIndex = 1 - confirmIndex;  // Toggle
    } else if (cmd == BTN_OK) {
      confirmed = true;

      if (confirmIndex == 0) {  // Confirm
        // Shift items left
        for (int i = index; i < itemCount - 1; i++) {
          items[i] = items[i + 1];
        }
        itemCount--;

        lcd.clear();
        lcd.print("Item Deleted");
        delay(1000);
        currentMode = MODE_SELECT;
        itemConfirmed = false;
        selectedIndex = 0;
      } else {  // Cancel
        currentMode = MODE_SELECT;
        itemConfirmed = true;  // ← go back to same item's option menu
      }
    }

    IrReceiver.resume();
    delay(200);
  }
}
