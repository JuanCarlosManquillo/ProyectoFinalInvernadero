/**
 * @file ProyectoFinalInvernadero.ino
 * @brief  Este programa implementa una máquina de estados para controlar un sistema de seguridad y monitoreo.
 *         El sistema utiliza diferentes sensores y actuadores para detectar intrusiones, medir temperatura
 *         y luz, y mostrar información en una pantalla LCD.
 * Autores: Karold Delgado 
            Juan C. Manquillo
 */
#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include "DHTStable.h"
#include <LiquidCrystal.h>
#include <Keypad.h>

#define LED_RED 10
#define LED_GREEN 8
#define LED_BLUE 9
#define BUZZER 7

#define PHOTO_SENSOR A0
#define TEMP_SENSOR A1

const float BETA = 3950;
const float GAMMA = 0.7;
const float RL10 = 50;

// Declaración de variables globales
char password[4] = {'1','1','1','1'};
char inPassword[4];
short int count = 0;
short int trycount = 0;
const uint8_t ROWS = 4;
const uint8_t COLS = 4;
int analogValue = 0 ;
float temp = 0.0;
int valorSensor=1;
unsigned int startTime=0;
unsigned int actualTime=0;
unsigned int actual= 0;
boolean estado;

// Definición del teclado (keypad) y su configuración
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

uint8_t colPins[COLS] = { 32, 34, 36, 38 }; // Pines conectados a C1, C2, C3, C4
uint8_t rowPins[ROWS] = { 24, 26, 28, 30 }; // Pines conectados a R1, R2, R3, R4
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Configuración de la pantalla LCD (LiquidCrystal)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
// Enumeración para representar los estados del programa
enum State
{
  inicioSesion = 0,
  bloqueado = 1,
  monitorLuz = 2,
  monitorTemperatura = 3,
  alerta = 4,
  alarma = 5
};

// Enumeración para representar las entradas del programa
enum Input
{
  clvCorrecta = 0,
  timeOut5 = 1,
  timeOut3 = 2,
  timeOut6 = 3,
  timeOut4 = 4,
  senTemp30 = 5,
  senLuz40 = 6,
  actBloqueo = 7,
  desconocido = 8
};
/**
 * @brief Monitorea la luz y actualiza la pantalla LCD.
 */
void pcdMonitorLuz(){
  lcd.setCursor(11,1);
  
  analogValue = analogRead(PHOTO_SENSOR);

  lcd.print(analogValue);

}
/**
 * @brief Monitorea la temperatura y actualiza la pantalla LCD.
 */
void pcdMonitorTemperatura(){
  lcd.setCursor(6,1);

  int analogValue = analogRead(TEMP_SENSOR);
  temp = 1 / (log(1 / (1023. / analogValue - 1)) / BETA + 1.0 / 298.15) - 273.15;
  lcd.print(temp);
  lcd.print("C");
  
}
// Creación de una nueva instancia de la máquina de estados
StateMachine stateMachine(6, 9);
// Variable para almacenar la última entrada de usuario
Input entrada;

// Creación de tareas asincrónicas para actualizar los datos de temperatura, humedad y luz en la pantalla LCD
AsyncTask asyncTaskTemp(2000, true, pcdMonitorTemperatura);
AsyncTask asyncTaskLuz(2000, true, pcdMonitorLuz);

/**
 * @brief Inicializa la máquina de estados y configura las transiciones.
 */
void setupStateMachine(){
  //Transiciones 
  stateMachine.AddTransition(inicioSesion, bloqueado, []() { return entrada == actBloqueo; });
  stateMachine.AddTransition(inicioSesion, monitorLuz, []() { return entrada == clvCorrecta; });

  stateMachine.AddTransition(bloqueado, inicioSesion, []() { return entrada == timeOut5; });

  stateMachine.AddTransition(monitorLuz, alerta, []() { return entrada == senLuz40; });
  stateMachine.AddTransition(monitorLuz, monitorTemperatura, []() { return entrada == timeOut3; });

  stateMachine.AddTransition(monitorTemperatura, monitorLuz, []() { return entrada == timeOut6; });
  stateMachine.AddTransition(monitorTemperatura, alarma, []() { return entrada == senTemp30; });
 
  stateMachine.AddTransition(alarma, monitorTemperatura, []() { return entrada == timeOut5; });

  stateMachine.AddTransition(alerta, monitorLuz, []() { return entrada == timeOut4; });

  stateMachine.SetOnEntering(bloqueado, pcdBloqueado);
  stateMachine.SetOnEntering(inicioSesion, pcdInicioSesion);
  stateMachine.SetOnEntering(monitorLuz, pcdLuz);
  stateMachine.SetOnEntering(monitorTemperatura, pcdTemp);
  stateMachine.SetOnEntering(alerta, pcdAlerta);
  stateMachine.SetOnEntering(alarma, pcdAlarma);

}
/**
 * @brief Configuración inicial del programa.
 */
void setup()
{
  // Inicialización del puerto serie
  Serial.begin(9600);
  //Mensaje inicio
  lcd.print("PASSWORD:");
  // Configuración de pines
  pinMode(LED_RED, OUTPUT); // LED indicador de sistema bloqueado
  pinMode(LED_BLUE, OUTPUT); // LED indicador de clave incorrecta
  pinMode(LED_GREEN, OUTPUT); // LED indicador de clave correcta
  pinMode(BUZZER, OUTPUT); // Buzzer
  
  // Iniciar tareas asíncronas
  asyncTaskTemp.Start();
  asyncTaskLuz.Start();
  // Configurar la máquina de estados
  setupStateMachine();  
  // Estado inicial
  stateMachine.SetState(inicioSesion, false, true);
  // Inicializar pantalla LCD
  lcd.begin(16, 2);
}
/**
 * @brief Bucle principal del programa.
 */
void loop(){
  
  // Actualizar la máquina de estados
  stateMachine.Update();
  int estado = stateMachine.GetState(); 
  //Serial.println(valorSensor);
  switch(estado){
    case bloqueado:
      entrada = static_cast<Input>(Input::actBloqueo);
      actualTime = millis();
      if(actualTime - startTime >= 5000){
        entrada = static_cast<Input>(Input::timeOut5);
      }
      break;
    case monitorLuz:
      asyncTaskLuz.Update();     
      if(analogValue < 40.0 ){ 
        delay(2000);
        entrada = static_cast<Input>(Input::senLuz40);
      }
      actualTime = millis();
      if(actualTime - startTime >= 3000){
        entrada = static_cast<Input>(Input::timeOut3);
        }  
      break;
    case alerta:
      actualTime = millis();
      if(actualTime - startTime >= 4000 ){
        entrada = static_cast<Input>(Input::timeOut4);
      }
      break;
    case monitorTemperatura:
      asyncTaskTemp.Update();
      if(temp > 30.00){
        entrada = static_cast<Input>(Input::senTemp30);
      }
      actualTime = millis();
      if(actualTime - startTime >= 6000){
        entrada = static_cast<Input>(Input::timeOut6);
      }
      break;
    case alarma:
      actualTime = millis();
      if(actualTime - startTime >= 5000 ){
        entrada = static_cast<Input>(Input::timeOut5);
      }
      break;

  }

}
/**
 * @brief Reinicia la pantalla LCD.
 */
void reiniciarLCD(){
    delay(300);
    lcd.setCursor(0,0);
    lcd.print("                ");
    lcd.setCursor(0,1);
    lcd.print("                ");
    lcd.setCursor(0,0);
}
/**
 * @brief Comprueba si la contraseña ingresada es correcta.
 * @return True si la contraseña es correcta, False en caso contrario.
 */
boolean comprobarClave(){
    for(int i=0; i<4; i++){
      if(inPassword[i] != password[i]){
        return false;
      }
    }
    return true;
}
/**
 * @brief Maneja el caso de una clave correcta.
 */
void claveCorrecta(){
  trycount=0;
  digitalWrite(LED_GREEN, HIGH);
  reiniciarLCD();
  lcd.print("PASSWORD CORRECT");
  
  // Reproducir tono de confirmación
  tone(BUZZER, 1800, 100);
  delay(100);
  tone(BUZZER, 2000, 100);
  delay(100);
  tone(BUZZER, 2200, 100);
  delay(100);
  tone(BUZZER, 2500, 350);
  delay(350);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_GREEN, LOW);
  reiniciarLCD();
}
/**
 * @brief Maneja el caso de una clave incorrecta.
 */
void claveIncorrecta(){
  digitalWrite(LED_BLUE, HIGH);
  lcd.setCursor(0,1);
  lcd.print("INCORRECTA >:(");

  // Reproducir tono de error
  tone(BUZZER, 1000, 200);
  delay(200);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_BLUE, LOW);
  reiniciarLCD();
  lcd.print("PASSWORD:");
  trycount++;
}
/**
 * @brief Maneja el caso de tiempo agotado para ingresar la contraseña.
 */
void tiempoAgotado(){
  digitalWrite(LED_BLUE, HIGH);
  lcd.setCursor(0,1);
  lcd.print("TIEMPO AGOTADO...");
 
  // Reproducir tono de aviso
  tone(BUZZER, 200, 150);
  delay(200);
  tone(BUZZER, 200, 150);
  delay(150);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_BLUE, LOW);
  reiniciarLCD();
  lcd.print("PASSWORD:");
  trycount++;
}
/**
 * @brief Inicia la sesión, maneja el ingreso de la contraseña y actualiza el display.
 * @return True si la sesión se inicia correctamente, False en caso contrario.
 */
bool initSesion() {
  
    if(count==4){
      // Caso de ingreso completo de la clave
      count = 0;
      estado = comprobarClave();
      if(estado == true){
        // Clave correcta
        claveCorrecta();
        
        delay(100);
        entrada = static_cast<Input>(Input::clvCorrecta);
        return true;
      }
      else{
        // Clave incorrecta
        claveIncorrecta();
        return false;
      }
    }
    return false;
}
/**
 * @brief Maneja el caso de bloqueo de la máquina de estados.
 */
void pcdBloqueado(){
  if (trycount == 3) {
    reiniciarLCD();
    digitalWrite(LED_RED, HIGH);
    lcd.print("*SIS.BLOQUEADO*");
    delay(5000);
    // Reproducir tono de bloqueo
    tone(BUZZER, 100, 200);
    digitalWrite(LED_RED, LOW);
    
    noTone(BUZZER);
    trycount++;
    actualTime = millis();
    reiniciarLCD();
    lcd.print("PASSWORD:");
  }
  else if (trycount > 3) {
    
    trycount = 0;
  }
}
/**
 * @brief Actualiza el display y maneja casos de bloqueo, contraseña correcta o incorrecta.
 * @return True si la operación fue exitosa, False en caso contrario.
 */
bool updateDisplay(){
    pcdBloqueado();
    return initSesion();
}
/**
 * @brief Maneja el inicio de sesión y muestra el teclado para ingresar la contraseña.
 */
void pcdInicioSesion(){
  bool estado;
  while(true){

    // Mostrar el cursor
    if(millis() / 250 % 2 == 0 && trycount < 3)
    {
      lcd.cursor();
    }
    else{
      lcd.noCursor();
    }
    estado = updateDisplay();
    if(estado == true){
      break;
    }
   
    // Si se presiona una tecla, se muestra un asterisco (*)
    char key = keypad.getKey();
    if (key) {
      // Guardar el tiempo en el que se presiona una tecla
      startTime = millis();
      lcd.print("*");
      inPassword[count] = key;
      count++;
    }
    // Actualizar el tiempo actual
    actualTime = millis();
 
    if(actualTime - startTime >= 4000 && count > 0){
      tiempoAgotado();
      startTime = 0; // Reiniciar el tiempo actual
      count = 0;
    }
  }
}
/**
 * @brief Maneja el caso de alerta cuando la luz es menor a 40.
 */
void pcdAlerta(){
  reiniciarLCD();
  lcd.print("*****ALERTA*****");
  lcd.setCursor(0,1);
  lcd.print("LUZ < 40");
  startTime = millis();
  int count = 0;
  while(count<6){
    digitalWrite(LED_RED, HIGH);
    delay(500);
    digitalWrite(LED_RED, LOW);
    delay(200);
    count++;
  }

  digitalWrite(LED_RED, LOW); // Asegurarse de que el LED esté apagado al salir de la función
}
/**
 * @brief Muestra el mensaje y la lectura del sensor de luz en la pantalla LCD.
 */
void pcdLuz(){
  reiniciarLCD();
  lcd.print("*PHOTORESISTSOR*");
  lcd.setCursor(0,1);
  lcd.print("Photocell:");
  startTime = millis();
}
/**
 * @brief Maneja el caso de alarma cuando la temperatura es mayor a 30.
 */
void pcdAlarma(){
  reiniciarLCD();
  lcd.print("*****ALARMA*****");
  lcd.setCursor(0,1);
  lcd.print("TEMPERATURA > 30");
  startTime = millis();
  // Activar el zumbador, tono de alerta
  tone(BUZZER, 1000, 5000); // Activar el tono a una frecuencia de 1000 Hz durante 200 milisegundos
  delay(5000);
  noTone(BUZZER); // Desactivar el tono en el zumbador
}
/**
 * @brief Muestra el mensaje y la lectura del sensor de temperatura en la pantalla LCD.
 */
void pcdTemp(){
  reiniciarLCD();
  lcd.print("**TEMP SENSOR**");
  lcd.setCursor(0,1);
  lcd.print("Temp: ");
  startTime = millis();
}