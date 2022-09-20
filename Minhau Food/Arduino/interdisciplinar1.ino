/*
Para o programa do APP, o sensor de distancia se encontra
a uma certa altura do pote, de forma que capta o conteudo
do pote de ração. Quando o pote esta cheio utilizamos a distancia
calculada, verificada pelo usuario no APP. Essa versão do código
funciona via bluetooth.
*/

//Inclusão de bibliotecas:
#include <Servo.h>  //Biblioteca para o Servo
#include "SoftwareSerial.h" //Biblioteca para trabalhar com bluetooth
#include <Wire.h> //Biblioteca para manipulação do protocolo I2C
#include <DS3231.h> //Biblioteca para usar o módulo RTC DS3231

//Definição de objetos:
DS3231 rtc; //Criação do objeto do tipo DS3231
RTCDateTime dataehora;  //Criação do objeto do tipo RTCDateTime
SoftwareSerial bluetooth(7, 5); //Criação de objeto do tipo SoftwareSerial e passando os pinos de entrada TX e RX

//Definição de constante:
const float vel = 34300.0; //Velocidade do som em cm/s

//Definindo variaveis (os numeros indicam o pino ao qual está ligado no Arduino):
#define ledV 13 //Led Verde
#define ledA 11//Led Azul
#define pinservo 4 //Pino do Microservo
#define trig 2 //Pino de acionador do sensor de distancia
#define echo 3 //Pino de echo do sensor de distancia

//Criação de variaveis globais:
int hora1, minuto1, hora2, minuto2, hora3, minuto3; //Essas variaveis armazenam as horas e minutos que foram definidas no aplicativo, são 3 horarios no total que podem ser configurados
int contDia=0, contSem=0, contMes=0;  //Essas variaveis contam os dias para 1 dia, 1 semana e 1 mês
int relDia, relSem, relMes; //Essas variaveis contém a quantidade de vezes que o animal se alimentou no dia, na semana e no mês

float vazio, cheio; //Essas variaveis armazenam os valores de distância da tijela vazia e cheia, respectivamente

bool autoHora; //Essa variavel contém verdadeiro quando o comedouro está automático e falso para quando foi definido um ou mais horários

String comando = "";  //Essa string vai receber o comando do aplicativo
Servo s; //Variavel do tipo Servo para controlar os angulos que o atuador irá fazer

//Struct com o relatório ATUAL do animal, ao final do dia, semana e mês ele começa com os valores em 0
struct Cadastro {
  int dia;
  int sem;
  int mes;
};

//Criação do objeto animal para conter seu relatório
struct Cadastro animal;

/*Esse procedimento padrão do Arduino inicia as configurações iniciais de várias coisas dentro do hardware e software. 
Sempre que o Arduino é ligado, esse procedimento é o primeiro a ser executado*/
void setup()
{
  rtc.begin();  //Inicia o módulo RTC
  bluetooth.begin(9600);  //Inicia módulo bluetooth
  pinMode(ledV, OUTPUT);  //Define o pino do Led verde como saída
  pinMode(ledA, OUTPUT);  //Define o pino do Led azul como saída
  pinMode(trig, OUTPUT);  //Define o pino do Trigger como saída
  pinMode(echo, INPUT); //Define o pino do Echo como entrada
  s.attach(pinservo); //Inicia o servo motor
  s.write(0); //Coloca o servo motor no ângulo 0
  rtc.setDateTime(__DATE__, __TIME__);  //Define a data e horário inicial do módulo como a data e horário no momento da compilação desse código no Arduino
  digitalWrite(ledV, 1);  //Liga Led verde 
  animal.dia=0; //Inicia o relatório do dia como 0
  animal.sem=0; //Inicia o relatório da semana como 0
  animal.mes=0; //Inicia o relatório do mês como 0
}

//Esse procedimento inicia o Trigger para que consiga calcular a distância
void chronoTrigger() {
  digitalWrite(trig, 0);  //Desliga Trigger
  delayMicroseconds(2); //Espera 2 microsegundos
  digitalWrite(trig, 1);  //Liga Trigger
  delayMicroseconds(10);  //Espera 10 microsegundos
  digitalWrite(trig, 0);  //Desliga Trigger
}

//Função que calcula a distância captada no sensor, depois retorna um valor float com a distancia medida
float calcDist() {
  unsigned long time = pulseIn(echo, 1); //Variavel que contém o tempo total no percurso da frequência sonora do sensor
  float distancia = time * 0.000001 * vel / 2.0;  //Variavel que transforma a distância em cm
  delay(500); //Espera 0,5 segundos
  return distancia; //Retorna a distância calculada em float
}

//Esse procedimento realiza a verificação se o animal está comendo e então, registra em seu relatório
void comer() {
  float distanciaAtual = calcDist();  //Variavel que chama função para conter a distância que o sensor está medindo no momento
  
  /*caso a distância atual seja menor que a distância registrada em tijela cheia, 
  significa que algo está na frente da ração, esse algo é o animal*/
  if (distanciaAtual < cheio) {
    delay(3000);  //Espera 3 segundos para garantir que é realmente o animal
    distanciaAtual=calcDist();  //Calcula novamente a distância atual

    //Caso continue com uma distância menor, então registra no relatório
    if (distanciaAtual < cheio) {

      //Caso a contagem de dias seja 0, então é registrado em seu relatório
      if(contDia == 0) {
        relDia++; //Aumenta o número de vezes que o animal comeu no dia
      }

      //Caso a contagem de dias da semana seja menor que 8, então é registrado em seu relatório
      if (contSem < 8) {
        relSem++; //Aumenta o número de vezes que o animal comeu na semana
      }

      //Caso a contagem de dias do mês seja menor que 31, então é registrado em seu relatório
      if (contMes < 31) {
        relMes++; //Aumenta o número de vezes que o animal comeu no mês
      } 
    }
  }  
}

//Esse procedimento abre e fecha o depósito de ração. O depósito fica aberto por 1 segundo
void encherPote() {

  //Servo motor gira abrindo o depósito de ração
  for (int i=0; i<=90; i++) {
    s.write(i); //Gira motor conforme i varia
    delay(15);  //Espera 0,015 segundos para o motor girar com mais precisão
  }
	
  delay(1000);  //Espera 1 segundo para despejar a ração
	 
  //Servo motor gira fechando o depósito de ração
  for (int i=90; i>=0; i--) {
    s.write(i); //Gira motor conforme i varia
    delay(15);  //Espera 0,015 segundos para motor girar com mais precisão
  }
}

/*Esse procedimento funciona como o cérebro da comunicação Bluetooth, 
ele recebe as requisições do aplicativo e executa um determinado comando*/
void cerebro() {
  char caraRec = bluetooth.read(); //Essa varíavel vai conter o byte enviado pelo aplicativo

  //Ignora caracteres nulos
  if (int(caraRec) == 0) {  
        return;
  }

  //A string global "comando" vai somando os caracteres recebidos
  comando += caraRec;

  //Quando o caractere recebido for ";", significa que o comando encerrou 
  if (caraRec == ';') {

    /*Cada comando no aplicativo possui alguns códigos para informar, por exemplo
    qual tela ou procedimento (na programação do aplicativo) está enviando o
    comando*/

    //Caso o código da tela seja "A"
    if (comando.substring(0,1) == "A") {
      String canal = comando.substring(1,1);  //A string canal recebe o código do canal
        
      //Caso seja "1"
      if (canal == "1") {
        vazio = calcDist(); //Define o estado da tijela de vazio como a distância atual medida pelo sensor
      } else {
          
        //Caso seja "2"
        if (canal == "2") {
          cheio = calcDist(); //Define o estado da tijela de cheio como a distância atual medida pelo sensor
        }
      }

      comando=""; //Esvazia a string de comando, pois foi encerrado o comando
    } else {

      //Caso o código da tela seja "B"
      if (comando.substring(0,1) == "B") {
        String canal = comando.substring(1,1);  //A string canal recebe o código do canal

        //Caso seja "2"
        if (canal == "2") {
          String valor = comando.substring(2, 1); //A string valor recebe o valor enviado pelo aplicativo

          //Se o valor é igual a "1"
          if (valor == "1") {
            autoHora=true;  //O estado do comedouro será definido como automático
          } else {

            //Se o valor é igual a "0"
            if (valor == "0") {
              autoHora=false; //O estado do comedouro será definido com os horários que o usuário enviou
            }
          }
        }

        comando=""; //Esvazia a string de comando, pois foi encerrado o comando
      } else {

        //Caso o código da tela seja "C"
        if (comando.substring(0, 1) == "C") {
          String canal = comando.substring(1, 1); //A string canal recebe o código do canal

          //Caso canal seja igual a "3"
          if (canal == "3") {
            String contador = comando.substring(3, 1);  //A string contador recebe qual horário foi configurado

            //Caso o contador seja "1"
            if (contador == "1") {
              String hora = comando.substring(5, 1) + comando.substring(7, 1);  //String hora recebe o valor da hora configurado
              String minuto = comando.substring(9, 1) + comando.substring(11, 1); //String minuto recebe o valor dos minutos configurados

              //No aplicativo foi definido que quando se apaga um dos horários definidos, ele envia "99"
              if(hora == "99") {
                //Estabelece como 00 a hora e minuto, mas não sera definido para meia noite pois outro procedimento não permite
                hora1=-1; 
                minuto1=-1;
              } else {
                //Se não, hora1 e minuto1 recebem as horas e minutos recebidos pelo aplicativo
                hora1=hora.toInt();
                minuto1=minuto.toInt();
              }
            } else {
              //Caso o contador seja "2"
              if (contador == "2") {
                String hora = comando.substring(5, 1) + comando.substring(7, 1);  //String hora recebe o valor da hora configurado
                String minuto = comando.substring(9, 1) + comando.substring(11, 1); //String minuto recebe o valor dos minutos configurados

                //No aplicativo foi definido que quando se apaga um dos horários definidos, ele envia "99"
                if(hora == "99") {
                  //Estabelece como 00 a hora e minuto, mas não sera definido para meia noite pois outro procedimento não permite
                  hora2=-1;
                  minuto2=-1;
                } else {
                  //Se não, hora2 e minuto2 recebem as horas e minutos recebidos pelo aplicativo
                  hora2=hora.toInt();
                  minuto2=minuto.toInt();
                }
              } else {
                //Caso o contador seja "3"
                if (contador == "3") {
                  String hora = comando.substring(5, 1) + comando.substring(7, 1);  //String hora recebe o valor da hora configurado
                  String minuto = comando.substring(9, 1) + comando.substring(11, 1); //String minuto recebe o valor dos minutos configurados

                  //No aplicativo foi definido que quando se apaga um dos horários definidos, ele envia "99"  
                  if(hora == "99") {
                    hora3=-1;
                    minuto3=-1;
                  } else {
                    //Se não, hora3 e minuto3 recebem as horas e minutos recebidos pelo aplicativo
                    hora3=hora.toInt();
                    minuto3=minuto.toInt();
                  }
                }
              }
            }
            comando = ""; //Esvazia a string comando, pois ele finalizou
          }
        } else {
          //Caso o código da tela seja "D"
          if (comando.substring(0, 1) == "D") {
            String canal = comando.substring(1, 1); //String canal recebe o código do canal

            //Caso canal seja "4"
            if (canal == "4") {
              String valor = comando.substring(2, 1); //A string valor recebe o código do que está sendo enviado

              //Caso valor seja "D"
              if (valor == "D") {
                bluetooth.println(relDia);  //Envia o número de vezes que o animal comeu no dia
              } else {
                //Caso valor seja "S"
                if (valor == "S") {
                  bluetooth.println(relSem);  //Envia o número de vezes que o animal comeu na semana
                } else {
                  //Caso valor seja "M"
                  if (valor == "M") {
                    bluetooth.println(relMes);  //Envia o número de vezes que o animal comeu no mês
                  }
                }
              }
            } else {

              //Caso canal seja "5"
              if (canal == "5") {
                String valor = comando.substring(2, 1); //String valor recebe o código do que está sendo enviado
                
                //Caso valor seja "A"
                if (valor == "A") {
                  //Define todos os valores de relatório como 0
                  relDia=0;
                  relSem=0;
                  relMes=0;
                }
              }
            }

            comando = ""; //Esvazia a string comando, pois ele finalizou
          } else {

            //Caso o código da tela seja "6"
            if (comando.substring(0, 1) == "6") {
              //As strings a seguir recebem o dia, mes, ano, hora e minutos do celular no momento que pareia
              String diaT = comando.substring(1, 2);
              String mesT = comando.substring(3, 2);
              String anoT = comando.substring(5, 4);
              String horaT = comando.substring(9, 2);
              String minutoT = comando.substring(11, 2);
              
              //Converte para numero int os valores em texto
              int dia=diaT.toInt(), mes=mesT.toInt(), ano=anoT.toInt(), hora=horaT.toInt(), minuto=minutoT.toInt();

              //Ajusta o tempo do módulo RTC como sendo o do celular do usuário
              rtc.adjust(dataehora(ano, mes, dia, hora, minuto, 0));
            }

            comando = ""; //Esvazia a string comando, pois ele finalizou
          }
        }
      }
    }
  }

}

/*Esse procedimento enche a tigela de acordo com as configurações de modo automático,
horários definidos pelo usuário e distância atual do sensor*/
void comidaAuto() {

  //Caso a varíavel que define o modo automático esteja como true
  if (autoHora == true) {
    float distanciaAtual = calcDist();  //A variavel recebe os valores de distância atuais
    
    //Caso a distância atual seja igual a distância configurada como "vazio"
    if (distanciaAtual == vazio) {
      do {
        encherPote(); //Chama procedimento que enche a tigela
      } while (distanciaAtual < cheio); //Enquanto a distância atual for menor que a distância da tigela cheia
    }
  } else {

    //Se a hora e os minutos forem iguais aos valores estabelecidos pelo usuário para o horário 1
    if (dataehora.hour == hora1 && dataehora.minute == minuto1) {
      float distanciaAtual = calcDist();  //Recebe a distância atual 
      
      //Se a distância atual for igual a distância de vazio
      if (distanciaAtual == vazio) {
      do {
        encherPote(); //Chama procedimento que enche a tigela
      } while (distanciaAtual < cheio); //Enquanto a distância atual for menor que a distância da tigela cheia
      }
    } else {

      //Se a hora e os minutos forem iguais aos valores estabelecidos pelo usuário para o horário 2
      if (dataehora.hour == hora2 && dataehora.minute == minuto2) {
        float distanciaAtual = calcDist();  //Recebe a distância atual 

        //Se a distância atual for igual a distância de vazio
        if (distanciaAtual == vazio) {
        do {
          encherPote(); //Chama procedimento que enche a tigela
        } while (distanciaAtual < cheio); //Enquanto a distância atual for menor que a distância da tigela cheia
        }
      } else {

        //Se a hora e os minutos forem iguais aos valores estabelecidos pelo usuário para o horário 3
        if (dataehora.hour == hora3 && dataehora.minute == minuto3) {
          float distanciaAtual = calcDist();  //Recebe a distância atual 

          //Se a distância atual for igual a distância de vazio
          if (distanciaAtual == vazio) {
          do {
            encherPote(); //Chama procedimento que enche a tigela
          } while (distanciaAtual < cheio); //Enquanto a distância atual for menor que a distância da tigela cheia
          }
        }
      }
    }
  }
}

//Esse procedimento é realizado sempre enquanto o Arduino estiver ligado
void loop()
{

  //Caso a hora atual seja 0, significa que o dia passou, logo
  if (now.hour() == 0) {

    //Aumenta o valor das variaveis de contador
    contDia++;
    contSem++;
    contMes++;
  }

  //Se o contador de dia for igual a 1
  if (contDia == 1) {
    //contDia volta a ser 0, pois é necessário que ele conte a duração de 1 dia
    contDia=0;
  }

  //Se o contador de dias da semana foi igual a 8
  if (contSem == 8) {
    //contSem volta a ser 0, pois é necessário que ele conte a duração de 1 semana em dias
    contSem=0;
  }

  //Se o contador de dias do mês for igual a 31
  if (contMes == 31) {
    //contMes vola a ser 0, pois é necessário que ele conte a duração de 1 mês em dias
    contMes=0;
  }

  //Chama procedimento para ativar Trigger
  chronoTrigger();

  //Se o bluetooth estiver disponível
  if (bluetooth.available() > 0) {
    //liga Led azul, indicando conexão
    digitalWrite(ledA, 1);

    //chama procedimento para receber requisições
    cerebro();
  } else {
    //Se não, desliga Led azul
    digitalWrite(ledA, 0);
    //Chama procedimento para verificar se o animal está comendo
    comer();

    //Chama procedimento para verificar se existe necessidade de encher a tigela
    comidaAuto();
  }
}