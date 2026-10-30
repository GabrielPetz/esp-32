const int ledPin = 4;

void setup() {
  // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(ledPin, HIGH);
  delay(10);
  digitalWrite(ledPin, LOW);
  delay(10);
}
