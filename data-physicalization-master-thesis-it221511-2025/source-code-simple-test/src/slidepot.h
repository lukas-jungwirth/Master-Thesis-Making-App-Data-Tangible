#ifndef SLIDEPOT_H
#define SLIDEPOT_H

int stdbyPin = 47;

bool delayStarted = false;
bool animationInProgress = false;

const int position_threshold = 10;
const int movement_threshold = 100;
int target_position = -1;
int slider_in_motion = -1;
unsigned long move_start_time = 0;
bool move_in_progress = false;
CRGB targetColor;  // For animation color


// const int testPin = 1;
void initSlidePot(ColumnsStruct& columns)
{

    for (size_t i = 0; i < columns.size(); i++)
    {
        Pot pot = columns.getPot(i);
        pinMode(pot.VAL, INPUT);
        pinMode(pot.IN1, OUTPUT);
        pinMode(pot.IN2, OUTPUT);
        pinMode(pot.PWM, OUTPUT);
        ledcSetup(i, 20000, 8);
        ledcAttachPin(pot.PWM, i);
    }

    // Initialize standby pin for motor driver
    pinMode(stdbyPin, OUTPUT);
}


int calculate_speed(int distance, bool going_up)
{
    int max_distance = 4095; // Update to reflect the actual range of the potentiometer
    int max_speed_up = 256;
    int min_speed_up = 180;
    int max_speed_down = 200;
    int min_speed_down = 120;

    // Determine the max and min speed based on the direction of movement
    int max_speed = going_up ? max_speed_up : max_speed_down;
    int min_speed = going_up ? min_speed_up : min_speed_down;

    // Calculate the proportional speed based on the distance to the target
    int speed = min_speed + (max_speed - min_speed) * ((float)distance / max_distance);

    // Ensure speed stays within the bounds of min_speed and max_speed
    speed = constrain(speed, min_speed, max_speed);

    return speed; // Return calculated speed without overriding
}


void move_to_position(ColumnsStruct& columns, int columnIndex, int new_pos, bool withAnimation = true)
{
	Serial.println("move column "+String(columnIndex)+"to position"+new_pos);
	new_pos = constrain(new_pos, 0, 4095);
    Pot pot = columns.getPot(columnIndex);
    int fader_pos = analogRead(pot.VAL);

    const int position_threshold = 10;
    const int movement_threshold = 50;

    unsigned long startTime = millis();
    CRGB columnColor = getColorFromStore(columnIndex);

    if (abs(fader_pos - new_pos) < movement_threshold)
    {
        return;
    }

    // Wait for fadeAnimation to complete if enabled
    if (withAnimation)
    {
        for (size_t i = 0; i < 5; i++)
        {
            fadeAnimation(columnIndex, columnColor);
        }
    }

    digitalWrite(stdbyPin, HIGH);  // Enable motor driver

    while (abs(fader_pos - new_pos) > position_threshold)
    {
        bool going_up = fader_pos < new_pos;
        int speed = calculate_speed(abs(fader_pos - new_pos), going_up);
		//Serial.println("speed: "+String(speed));

        if (going_up)
        {
            digitalWrite(pot.IN1, HIGH);
            digitalWrite(pot.IN2, LOW);
            ledcWrite(columnIndex, speed);
        }
        else
        {
            digitalWrite(pot.IN1, LOW);

            digitalWrite(pot.IN2, HIGH);
            ledcWrite(columnIndex, speed);
        }

        fader_pos = analogRead(pot.VAL);

        if (millis() - startTime > 5000)
        {
            break;
        }
    }
	Serial.println("move column "+String(columnIndex)+"to position"+fader_pos+"finished");

    ledcWrite(columnIndex, 0);  // Stop PWM for this slider
    digitalWrite(stdbyPin, LOW);  // Disable motor driver (standby)

    if (withAnimation)
    {
        setSolidColor(columnIndex, columnColor);
    }
}

void moveColumnUpandDown(ColumnsStruct& columns, int columnIndex)
{
    Pot pot = columns.getPot(columnIndex);
    int slidePotValue = analogRead(pot.VAL);
    int downPos = slidePotValue - 800;
    int upPos = slidePotValue + 800;
    for (size_t i = 0; i < 7; i++)
    {
        if (i % 2 == 0)
        {
            if (downPos < 0)
            {
                downPos = 0;
            }
            move_to_position(columns, columnIndex, downPos, false);
        }
        else
        {
            if (upPos > 4095)
            {
                upPos = 4095;
            }
            move_to_position(columns, columnIndex, upPos, false);
        }
        delay(1000);
    }
    delay(500);
    move_to_position(columns, columnIndex, slidePotValue, false);
}




#endif // LED_H