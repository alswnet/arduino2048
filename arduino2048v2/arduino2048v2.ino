#include <Adafruit_GFX.h> // Libreria de graficos
#include <Adafruit_TFTLCD.h> // Libreria de LCD
#include <SparkFun_APDS9960.h> //Libreria del Sensor de Gestos
#include <TouchScreen.h>     // Libreria del panel tactil
#include <Wire.h>//Libreria para I2C necesario para el sensor de Bateria

#define LCD_CS A3 // Definimos los pines del LCD
#define LCD_CD A2 // para poder visualizar elementos graficos
#define LCD_WR A1
#define LCD_RD A0
//OJO OJO Se modifico el escujo cambiando el pin fisico de A4 hacia 12
#define LCD_RESET 12

// Pines necesarios para los 4 pines del panel tactil
#define YP A1 // Pin analogico A1 para ADC
#define XM A2 // Pin analogico A2 para ADC
#define YM 7
#define XP 6

#define MAX17043_ADDRESS 0x36  // R/W =~ 0x6D/0x6C

#define CFondo 0xbd74 //RGB656 

static int Colores[14] = {
  0xce16,//0 0
  0xED20,
  0x1631,
  0xF900,
  0xF81F,
  0x07F7,
  0x6006,
  0x01BF,
  0xF900,
  0x87FB,
  0x6EF7,
  0xED20,
  0x6EF7,
  0xA44E
};

//Enumeracion de los estados del automata finito de juego
enum ESTADO_JUEGO {
  ES_INACTIVO, ES_DESPLAZAR, ES_COMBINAR, ES_CREAR, ES_REGRESAR
};

//Enumeracion de los comandos de control de juego
enum COMANDO_CONTROL {
  CC_IZQUIERDA, CC_DERECHA, CC_ARRIBA, CC_ABAJO, CC_REINICIAR, CC_REGRESA
};

enum DIR_MOVIMIENTO {
  DM_IZQ, DM_DER, DM_ARR, DM_ABA,
};

enum ESTADO_PASOS {
  ES_PARAR, ES_SEGIR, ES_MOVIO, ES_SUMA
};

Adafruit_TFTLCD Pantalla(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET); // Instancia LCD

//Instancia de objeto para la clase del sensor de gestos
SparkFun_APDS9960 Sensor = SparkFun_APDS9960();

short TS_MINX = 150; // Coordenadas del panel tactil para delimitar
short TS_MINY = 120; // el tamaño de la zona donde podemos presionar
short TS_MAXX = 850; // y que coincida con el tamaño del LCD
short TS_MAXY = 891;

// Para mejor precision de la presion realizada, es necesario
// medir la resistencia entre los pines X+ y X-.
// En Shield TFT 2.4" LCD se mide entre los pines A2 y 6
// Instancia del panel tactil (Pin XP, YP, XM, YM, Resistencia del panel)
TouchScreen Tactil = TouchScreen(XP, YP, XM, YM, 364);

static int GrosorX = Pantalla.width() / 4 ;
static int GrosorY = GrosorX;
//int GrosorY = Pantalla.height() / 4;
//x y

int ValorCelda[4][4] = {
  { 1, 0, 0, 0},
  { 1, 0, 0, 0},
  { 2, 0, 0, 0},
  { 3, 0,  0,  0}
};

int ValorCeldaPasada[4][4] = {
  { 0, 0, 0, 0},
  { 0, 0, 0, 0},
  { 0, 0, 0, 0},
  { 0, 0, 0, 0}
};

int ValorUnion[4][4] = {
  { 0, 0, 0, 0},
  { 0, 0, 0, 0},
  { 0, 0, 0, 0},
  { 0, 0, 0,  0}
};

int x, x_ini, x_fin, x_inc, x_bus;
int y, y_ini, y_fin, y_inc, y_bus;

int X; // Variables que almacenaran la coordenada
int Y; // X, Y donde presionemos y la variable Z
int Z; // almacenara la presion realizada

int Guardado = 0;

float Puntos = 0;
float PPuntos = 10;
float PMenos = 0;

float batPercentage;
float pasPercentage = 10;

ESTADO_JUEGO EstadoJuego = ES_INACTIVO; //Estado actual del automata
DIR_MOVIMIENTO DirMovimiento;            //Direccion de movimiento cuando se desplaza
ESTADO_PASOS Continuamos =  ES_PARAR;

void setup() {
  //Se inicializa el puerto serie
  Serial.begin(9600);
  Serial.println("Iniciando");

  //La semilla del generador de numeros pseudo-aleatorios se alimenta con un pin de ADC abierto
  randomSeed(analogRead(A5));
  Serial.println("Bajarando el mazo");

  //Se inicializa el sensor de gestos
  Sensor.init();
  Sensor.enableGestureSensor(false);
  Sensor.setLEDDrive(LED_DRIVE_12_5MA);  //Este par de calibraciones se hace porque el sensor esta muy sensible
  Sensor.setGestureGain(GGAIN_1X);       //por default. Mermar la ganancia y la luz mejora la respuesta.
  Serial.println("Activando el sensor de Gestos");

  Wire.begin();  // Start I2C
  delay(100);
  configMAX17043(32);  // Configure the MAX17043's alert percentage
  qsMAX17043();  // restart fuel-gauge calculations
  Serial.println("Activando Sensor de Voltaje de Vateria");

  //Iniciar la pantalla con el driver correcto
  Pantalla.begin(0x9325); // Iniciamos el LCD especificando el controlador ILI9341.
  Pantalla.setRotation(2);
  Pantalla.fillScreen(CFondo);
  Serial.println("Activando la pantalla");
  Serial.print(Pantalla.width());
  Serial.print("x");
  Serial.println(Pantalla.height());

  ReiniciarJuego();
  Dibujar();
  delay(100);
}

void loop() {

  //Leer las entradas del juego
  LeerSerial();//Buscas comando por el puero Serial
  LeerGestos();//Busca comandos por el sensor de gestos
  LecturaPanel();

  // batPercentage = percentMAX17043();

  switch (EstadoJuego) {
    case ES_INACTIVO:

      break;
    case ES_DESPLAZAR:
      Serial.println("Empezando..");
      EscojerDirecion();
      if (SePuedeMover()) {
        if (Guardado == 0) {
          Guardado = 1;
          GuardasPasado();
        }
        Moviendo();
      }
      else {
        switch (Continuamos) {
          case ES_SEGIR:
            EstadoJuego = ES_CREAR;
            break;
          case ES_PARAR:
            EstadoJuego = ES_INACTIVO;
            break;
          case ES_MOVIO:
            EstadoJuego = ES_CREAR;
            break;
          case ES_SUMA:
            EstadoJuego = ES_CREAR;
            break;
        }
        Continuamos = ES_PARAR;
      }
      break;
    case ES_CREAR:
      crearCelda();
      Guardado = 0;
      EstadoJuego = ES_INACTIVO;
      LimpiarUnion();
      //MostrarMatix(ValorUnion);
      break;
  }

  DibujarPuntos();//Dibuja el Marcador
  DibujarBateria();//Dibuja la bateria restante
}

void LimpiarUnion() {
  for (x = 0; x < 4; x++) {
    for (y = 0; y < 4 ; y++) {
      ValorUnion[x][y] = 0;
    }
  }
}

void MostrarMatix(int M[4][4]) {
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4 ; y++) {
      Serial.print(M[x][y]);
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println();
}

int SePuedeMover() {
  for (x = x_ini; x != x_fin; x += x_inc) {
    for (y = y_ini; y != y_fin; y += y_inc) {
      if (ValorUnion[x][y] == 0 &&
          ValorCelda[x][y] > 0 &&
          (ValorCelda[x + x_bus][y + y_bus] == 0 ||
           (ValorCelda[x + x_bus][y + y_bus] == ValorCelda[x][y]
            && ValorUnion[x + x_bus][y + y_bus] == 0))) {
        Serial.print("Segir Moviendo ");
        Serial.print(x);
        Serial.print("-");
        Serial.println(y);
        return 1;
      }
    }
  }
  return 0;
}
void EscojerDirecion() {
  Serial.println("Direcion..");
  switch (DirMovimiento) {
    case DM_ARR:
      x_ini = 0; x_fin = 4; x_inc = 1; x_bus = 0;
      y_ini = 1; y_fin = 4; y_inc = 1; y_bus = -1;
      break;
    case DM_ABA:
      x_ini = 0; x_fin = 4; x_inc = 1; x_bus = 0;
      y_ini = 2; y_fin = -1; y_inc = -1; y_bus = 1;
      break;
    case DM_IZQ:
      x_ini = 1; x_fin = 4; x_inc = 1; x_bus = -1;
      y_ini = 0; y_fin = 4; y_inc = 1; y_bus = 0;
      break;
    case DM_DER:
      x_ini = 2; x_fin = -1; x_inc = -1; x_bus = 1;
      y_ini = 0; y_fin = 4; y_inc = 1; y_bus = 0;
      break;
  }
}

void Moviendo() {
  for (x = x_ini; x != x_fin; x += x_inc) {
    for (y = y_ini; y != y_fin; y += y_inc) {

      if (ValorCelda[x][y] > 0 ) {/*&& ValorUnion[x][y] == 0*/
        if (ValorCelda[x + x_bus][y + y_bus] == 0) {
          ValorCelda[x + x_bus][y + y_bus] = ValorCelda[x][y];
          ValorCelda[x][y] = 0;
          Continuamos = ES_MOVIO;
          DibujarCuadro(x, y, ValorCelda[x][y]);
          DibujarCuadro(x + x_bus, y + y_bus, ValorCelda[x + x_bus][y + y_bus]);
        }
        else if (ValorCelda[x + x_bus][y + y_bus] == ValorCelda[x][y]) {
          ValorCelda[x][y] = 0;
          ValorCelda[x + x_bus][y + y_bus]++;
          ValorUnion[x + x_bus][y + y_bus] = 1;
          PMenos = pow(2, ValorCelda[x + x_bus][y + y_bus]);
          Puntos += PMenos;
          Continuamos = ES_SUMA;
          DibujarCuadro(x, y, ValorCelda[x][y]);
          DibujarCuadro(x + x_bus, y + y_bus, ValorCelda[x + x_bus][y + y_bus]);
        }
      }
    }
  }
}

void DibujarBateria() {
  if (batPercentage != pasPercentage) {
    pasPercentage = batPercentage;
    Pantalla.fillRect(GrosorX * 2 + 3, GrosorY * 4 + 55, GrosorX * 2 - 3 * 2, GrosorY - 3 * 2 , CFondo);
    Pantalla.setCursor(GrosorX * 2 + 6, GrosorY * 4 + 60);
    Pantalla.setTextSize(2);
    Pantalla.print("B=");
    Pantalla.print(batPercentage, 0);  // Print the battery percentage
    Pantalla.println("%");
  }
}

void DibujarPuntos() {
  if (Puntos != PPuntos) {
    PPuntos = Puntos;
    Pantalla.fillRect(GrosorX * 2 + 3, GrosorY * 4 + 3, GrosorX * 2 - 3 * 2, GrosorY - 3 * 2 , CFondo);
    Pantalla.setCursor(GrosorX * 2.5, GrosorY * 4 + 10);
    Pantalla.setTextSize(2);
    Pantalla.print("SCORE");
    Pantalla.setCursor(GrosorX * 2 + 3, GrosorY * 4.5);
    Pantalla.setTextSize(3); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(Puntos, 0);
  }
}

void DibujarPuntosMAX() {
  if (Puntos != PPuntos) {
    PPuntos = Puntos;
    Pantalla.fillRect(GrosorX * 1 + 3, GrosorY * 4 + 3, GrosorX * 2 - 3 * 2, GrosorY - 3 * 2 , CFondo);
    Pantalla.setCursor(GrosorX * 2.5, GrosorY * 4 + 10);
    Pantalla.setTextSize(2);
    Pantalla.print("HIGH SCORE");
    Pantalla.setCursor(GrosorX * 2 + 3, GrosorY * 4.5);
    Pantalla.setTextSize(3); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(Puntos, 0);
  }
}

void DibujarCuadro(int x, int y, int potencia) {
  Pantalla.fillRect(x * GrosorX + 3 , y * GrosorY + 3 , GrosorX - 3 * 2  , GrosorY  - 3 * 2, Colores[potencia]);
  if (potencia > 9) {
    Pantalla.setCursor(x * GrosorX + 6, y * GrosorY + 25);
    Pantalla.setTextSize(2); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(pow(2, potencia), 0);
  }
  else if (potencia > 6) {
    Pantalla.setCursor(x * GrosorX + 5, y * GrosorY + 20);
    Pantalla.setTextSize(3); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(pow(2, potencia), 0);
  }
  else  if (potencia > 3) {
    Pantalla.setCursor(x * GrosorX + 8, y * GrosorY + 15);
    Pantalla.setTextSize(4); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(pow(2, potencia), 0);
  }
  else  if (potencia > 0) {
    Pantalla.setCursor(x * GrosorX + 15, y * GrosorY + 10);
    Pantalla.setTextSize(6); // Definimos tamaño del texto.
    Pantalla.setTextColor(0x00FFFF);
    Pantalla.print(pow(2, potencia), 0);
  }
}

void Dibujar() {
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      DibujarCuadro(x, y, ValorCelda[x][y]);
    }
  }
}

void LeerGestos() {
  //Se procede a leer el sensor y se genera un comando a la logica de juego
  //segun el gesto detectado (si se detecto uno)
  if (Sensor.isGestureAvailable()) {
    switch (Sensor.readGesture()) {
      case DIR_LEFT:
        Accion(CC_IZQUIERDA);
        break;
      case DIR_RIGHT:
        Accion(CC_DERECHA);
        break;
      case DIR_UP:
        Accion(CC_ARRIBA);
        break;
      case DIR_DOWN:
        Accion(CC_ABAJO);
      case DIR_FAR:
        Accion(CC_REINICIAR);
        break;
    }
  }
}

void LeerSerial() {
  if (Serial.available()) {
    char caracter = Serial.read();
    Serial.print("Mover...");
    switch (caracter) {
      case 'w':
        Accion(CC_ARRIBA);
        Serial.println("Ariba");
        break;
      case 'a':
        Accion(CC_IZQUIERDA);
        Serial.println("Izquierda");
        break;
      case 's':
        Accion(CC_ABAJO);
        Serial.println("Abajo");
        break;
      case 'd':
        Accion(CC_DERECHA);
        Serial.println("Derecha");
        break;
      case 'r':
        Accion(CC_REINICIAR);
        Serial.println("Reiniciar");
        break;
    }
  }
}

void Accion(COMANDO_CONTROL comando) {
  if (EstadoJuego != ES_INACTIVO) return;
  switch (comando) {
    case CC_IZQUIERDA:
      EstadoJuego = ES_DESPLAZAR;
      DirMovimiento = DM_IZQ;
      break;
    case CC_DERECHA:
      EstadoJuego = ES_DESPLAZAR;
      DirMovimiento = DM_DER;
      break;
    case CC_ARRIBA:
      EstadoJuego = ES_DESPLAZAR;
      DirMovimiento = DM_ARR;
      break;
    case CC_ABAJO:
      EstadoJuego = ES_DESPLAZAR;
      DirMovimiento = DM_ABA;
      break;
    case CC_REINICIAR:
      ReiniciarJuego();
      break;
    case CC_REGRESA:
      RegresarMovimiento();
      break;
  }
}

void RegresarMovimiento() {
  Serial.println("Regresando");
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      ValorCelda[x][y] = ValorCeldaPasada[x][y];
    }
  }
  Puntos -= PMenos;
  Dibujar();
}

void GuardasPasado() {
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      ValorCeldaPasada[x][y] = ValorCelda[x][y];
    }
  }
}

void ReiniciarJuego() {
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      ValorCelda[x][y] = 0;
    }
  }
  Puntos = 0;
  EstadoJuego = ES_INACTIVO;
  Pantalla.fillScreen(CFondo);
  crearCelda();
  crearCelda();
  Dibujar();
}

void crearCelda() {
  bool Encontrado = false;
  int XR;
  int YR;
  do {
    XR = random(4);
    YR = random(4);
    if (ValorCelda[XR][YR] == 0) {
      ValorCelda[XR][YR] = (rand() % 20) / 19 + 1;;
      Encontrado = true;
    }
    else {
      Serial.print(" .. Buscando .. ");
    }
  } while (!Encontrado);
  Serial.println();
  Serial.print("Creando ");
  Serial.print(XR);
  Serial.print("-");
  Serial.println(YR);
  DibujarCuadro(XR, YR, ValorCelda[XR][YR]);
}

/*
  percentMAX17043() returns a float value of the battery percentage
  reported from the SOC register of the MAX17043.
*/
float percentMAX17043()
{
  unsigned int soc;
  float percent;

  soc = i2cRead16(0x04);  // Read SOC register of MAX17043
  percent = (byte) (soc >> 8);  // High byte of SOC is percentage
  percent += ((float)((byte)soc)) / 256; // Low byte is 1/256%

  return percent;
}

/*
  configMAX17043(byte percent) configures the config register of
  the MAX170143, specifically the alert threshold therein. Pass a
  value between 1 and 32 to set the alert threshold to a value between
  1 and 32%. Any other values will set the threshold to 32%.
*/
void configMAX17043(byte percent)
{
  if ((percent >= 32) || (percent == 0)) // Anything 32 or greater will set to 32%
    i2cWrite16(0x9700, 0x0C);
  else
  {
    byte percentBits = 32 - percent;
    i2cWrite16((0x9700 | percentBits), 0x0C);
  }
}

/*
  qsMAX17043() issues a quick-start command to the MAX17043.
  A quick start allows the MAX17043 to restart fuel-gauge calculations
  in the same manner as initial power-up of the IC. If an application's
  power-up sequence is very noisy, such that excess error is introduced
  into the IC's first guess of SOC, the Arduino can issue a quick-start
  to reduce the error.
*/
void qsMAX17043()
{
  i2cWrite16(0x4000, 0x06);  // Write a 0x4000 to the MODE register
}

/*
  i2cRead16(unsigned char address) reads a 16-bit value beginning
  at the 8-bit address, and continuing to the next address. A 16-bit
  value is returned.
*/
unsigned int i2cRead16(unsigned char address)
{
  int data = 0;

  Wire.beginTransmission(MAX17043_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(MAX17043_ADDRESS, 2);
  while (Wire.available() < 2)
    ;
  data = ((int) Wire.read()) << 8;
  data |= Wire.read();

  return data;
}

/*
  i2cWrite16(unsigned int data, unsigned char address) writes 16 bits
  of data beginning at an 8-bit address, and continuing to the next.
*/
void i2cWrite16(unsigned int data, unsigned char address)
{
  Wire.beginTransmission(MAX17043_ADDRESS);
  Wire.write(address);
  Wire.write((byte)((data >> 8) & 0x00FF));
  Wire.write((byte)(data & 0x00FF));
  Wire.endTransmission();
}

void LecturaPanel() {
  TSPoint p = Tactil.getPoint(); // Realizamos lectura de las coordenadas

  pinMode(XM, OUTPUT); // La librería utiliza estos pines como entrada y salida
  pinMode(YP, OUTPUT); // por lo que es necesario declararlos como salida justo
  // despues de realizar una lectura de coordenadas.

  // Mapeamos los valores analogicos leidos del panel tactil (0-1023)
  // y los convertimos en valor correspondiente a la medida del LCD 320x240
  X = map(p.x, TS_MAXX, TS_MINX,  Pantalla.width(), 0);
  Y = map(p.y, TS_MAXY, TS_MINY,  Pantalla.height(), 0);
  Z = p.z;
  if (Z > 0 && Z < 1000) {
    Serial.print("Z");
    Serial.println(Z);
    Accion(CC_REGRESA);
  }
}
