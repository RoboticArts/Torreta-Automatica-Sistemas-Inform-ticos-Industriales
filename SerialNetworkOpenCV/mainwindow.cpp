#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialog.h"
#include "network.h"
#include <QCloseEvent>

/*Puerto serie */
#include "portwindow.h"
#include <QMessageBox>
#include <QKeyEvent>
#include <QtSerialPort/QSerialPortInfo>

#include<QtCore>
#include<QMessageBox>
#include <qfiledialog.h>


//Valores del objeto mínimo a detectar
#define MIN_OBJECT_X 100
#define MIN_OBJECT_Y 100

//Resolucion de la camara
#define CAMERA_WIDHT 640
#define CAMERA_HEIGHT 480


extern Dialog *d;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    thread = new MasterThread;

    /*Puerto serie */
    ui->LabelInfo->hide(); // Puerto serie
    connect(ui->ButtonSerialPort , SIGNAL(clicked(bool)),this, SLOT(SettingSerialPort()));
    connect(thread, SIGNAL(response(QString)), this, SLOT(showResponse(QString)));
    connect(thread, SIGNAL(error(QString)), this, SLOT(processError(QString)));
    connect(thread, SIGNAL(timeout(QString)), this, SLOT(processTimeout(QString)));

    /*Network*/
    ui -> pushButtonShoot -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");
    ui -> pushButtonArmed -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");
    EnableDisableMainMenu(0);

    server = new CServer(this);
    client = new CClient(this);

    connect(server                                , SIGNAL(newConnection()), this, SLOT(ServerNewConnection()));

    connect(ui->SliderX, SIGNAL(valueChanged(int)), this, SLOT(SliderXSend())); //Es SLOT y funcion
    connect(ui->SliderY, SIGNAL(valueChanged(int)), this, SLOT(SliderYSend())); //Es SLOT y funcion
    connect(ui->pushButtonArmed, SIGNAL(clicked(bool)), this, SLOT(ArmedSendClicked()));
    connect(ui->pushButtonShoot, SIGNAL(pressed()), this, SLOT(ShootSendOne()));
    connect(ui->pushButtonShoot, SIGNAL(released()),this,SLOT(ShootSendZero()));

    connect(client, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(ClientStateChanged(QAbstractSocket::SocketState)));
    connect(client, SIGNAL(hostFound()),this, SLOT(ClientHostFound()));
    connect(client, SIGNAL(connected()),this, SLOT(ClientConnected()));
    connect(client, SIGNAL(error(QAbstractSocket::SocketError)),this, SLOT(ClientError(QAbstractSocket::SocketError)));
    connect(client, SIGNAL(readyRead()), this, SLOT(ClientRead()));



    /* OpenCV */

    frameTimer = new QTimer(this);

    //Se muestra un icono cuando no hay cámara
    QPixmap pixmap( ":digital-camera-icon.png" );
    ui -> LabelCapture -> setPixmap(pixmap);
    ui -> LabelCapture->setAlignment(Qt::AlignCenter);

    connect(ui->pushButtonOpenCloseCamera, SIGNAL(clicked(bool)), this, SLOT(OpenCloseCamera())); //Enciende o apaga el modo automatico
    connect(frameTimer, SIGNAL(timeout()), this, SLOT(processFrameAndUpdateGUI())); //Captura, analiza y muetra por pantalla el frame
    //Para no saturar el puerto serie, se enviará un nuevo valor cuando se añada un nuevo valor EditTextPlain
    //Emplea la variable privada "position que se actualiza en cada frame.
    connect(ui->plainTextEditCamera, SIGNAL(textChanged()), this, SLOT(CameraToSerialPort()));


}

MainWindow::~MainWindow()
{
    delete ui;

    delete server;
    delete client;
}

/////////////////////////////////// NETWORK ////////////////////////////////////////////


void MainWindow::ServerActivateDeactivate(void)
{

    //Impide el modo servidor si no hay COM detectado
    if(!port.isEmpty())  //Si el puerto no está vacio, hay COM detectado (si está vacio devuelve un 1)
    {
        if(server->isListening())
        {
            d -> EditPlainTextServer(QString("User: Deactivate") + '\n');
            server->close();
        }
        else
        {
            d -> EditPlainTextServer(QString("User: Activate "));

            if(server->listen(QHostAddress::Any, d->getLineEditServerPort().toInt()))
                d -> EditPlainTextServer(QString("OK\n"));
            else
                d -> EditPlainTextServer(QString("NO OK\n"));
        }
    }

    else
    {
    d->ShowMessageBox(); //Muestra la caja de informacion en la vetana de configuracion
    }

}

void MainWindow::ServerNewConnection(void)
{
    d->EditPlainTextServer(QString("Signal: newConnection()") + '\n');
}

void MainWindow::ServerRead(void)
{

}


void MainWindow::ClientConnectDisconnect(void)
{
    if(client->isOpen())
    {
        d ->EditPlainTextClient(QString("User: Disconnect") + '\n');

        d -> LabelClientLocalAddress(QString("Local Address = ?"));
        d -> LabelClientLocalPort(QString("Local Port = ?"));
        client->close();
    }

    else
    {
        d -> EditPlainTextClient(QString("User: Connect") + '\n');

        client -> connectToHost(d -> getLineEditClientRemoteAdress(), d-> getLineEditClientRemotePort().toInt());
        }
}

void MainWindow::ClientStateChanged(QAbstractSocket::SocketState state)
{
    QString state_text("Signal: stateChanged( ");
    switch (state)
    {
        case QAbstractSocket::UnconnectedState:
            state_text.append("UnconnectedState )");
            break;
        case QAbstractSocket::HostLookupState:
            state_text.append("HostLookupState )");
            break;
        case QAbstractSocket::ConnectingState:
            state_text.append("ConnectingState )");
            break;
        case QAbstractSocket::ConnectedState:
            state_text.append("ConnectedState )");
            break;
        case QAbstractSocket::BoundState:
            state_text.append("BoundState )");
            break;
        case QAbstractSocket::ClosingState:
            state_text.append("ClosingState )");
            break;
        case QAbstractSocket::ListeningState:
            state_text.append("ListeningState )");
            break;
    }
    d -> EditPlainTextClient(state_text + '\n');
}

void MainWindow::ClientHostFound(void)
{
    d -> EditPlainTextClient(QString("Signal: hostFound()") + '\n');
}

void MainWindow::ClientConnected(void)
{
    d -> EditPlainTextClient(QString("Signal: connected()") + '\n');

    d -> LabelClientLocalAddress(QString("Local Address = ") + client->localAddress().toString());
    d -> LabelClientLocalPort(QString("Local Port = ")    + QString::number(client->localPort()));

    d -> LineEditClientRemoteAddress(QString(client->peerAddress().toString()));
    d -> LineEditClientRemotePort(QString::number(client->peerPort()));
}

void MainWindow::ClientError(QAbstractSocket::SocketError error)
{
    d -> EditPlainTextClient(QString("Signal: error( ") + client->errorString() + " )\n");
}


bool MainWindow::SliderXSend(void)
{
    if(d->getStateClient()) //Siempre que esta máquina actue como maquina remota se envian datos por puerto seire
    {
        QString text = "X" + QString::number(ui -> SliderX -> value());
        QByteArray data = text.toUtf8();
        return client->write(data) == data.size();
    }
    return 0;
}

bool MainWindow::SliderYSend(void)
{
    if(d->getStateClient()) //Siempre que esta máquina actue como maquina remota se enviar datos por puerto serie
    {
        QString text = "Y" + QString::number(ui -> SliderY -> value());
        QByteArray data = text.toUtf8();
        return client->write(data) == data.size();
    }
    return 0;
}


bool MainWindow::ShootSendZero()
{
    if(d->getStateClient()) //Siempre que esta máquina actue como maquina REMOTA
    {
        ui -> pushButtonShoot -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");
        QString text = "S0";
        QByteArray data = text.toUtf8();
        return client->write(data) == data.size();
    }

    if(!getEnableDisableSliderButton()) //Si actua en MODO MANUAL
    {
        ui -> pushButtonShoot -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");

        SerialPortWrite("S0\n"); //Escribe por el puerto serie
    }

    return 0;
}


bool MainWindow::ShootSendOne()
{
    if(d->getStateClient() ) //Siempre que esta máquina actue como maquina REMOTA
    {
        ui -> pushButtonShoot -> setStyleSheet("background-color: rgba(0,255,0,0.6);  border-style: outset;");
        QString text = "S1";
        QByteArray data = text.toUtf8();
        return client->write(data) == data.size();
    }

    if(!getEnableDisableSliderButton()) //Si actua en MODO MANUAL
    {
        ui -> pushButtonShoot -> setStyleSheet("background-color: rgba(0,255,0,0.6);  border-style: outset;");

        SerialPortWrite("S1\n"); //Escribe por el puerto serie
    }

    return 0;
}


bool MainWindow::ArmedSendClicked()
{
     QString text = ui -> pushButtonArmed -> text();

    if(d->getStateClient()) //Siempre que esta máquina actue como maquina REMOTA
    {

        if(text == "ARMAR")
        {
            ui -> pushButtonArmed -> setStyleSheet("background-color: rgba(0,255,0,0.6);  border-style: outset;");
            ui -> pushButtonArmed -> setText("ARMADO");
            QString text = "A1";
            QByteArray data = text.toUtf8();
            return client->write(data) == data.size();
        }
        else
        {
            ui -> pushButtonArmed -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");
            ui -> pushButtonArmed -> setText("ARMAR");
            QString text = "A0";
            QByteArray data = text.toUtf8();
            return client->write(data) == data.size();
        }
    }

    if(!getEnableDisableSliderButton()) //Si actua en MODO MANUAL
    {
        if(text == "ARMAR")
        {
            ui -> pushButtonArmed -> setStyleSheet("background-color: rgba(0,255,0,0.6);  border-style: outset;");
            ui -> pushButtonArmed -> setText("ARMADO");

            SerialPortWrite("A1\n"); //Escribe por el puerto serie
        }
        else
        {
            ui -> pushButtonArmed -> setStyleSheet("background-color: rgba(255,0,0,0.6);  border-style: outset;");
            ui -> pushButtonArmed -> setText("ARMAR");

            SerialPortWrite("A0\n"); //Escribe por el puerto serie

        }
    }

    return 0;
}






void MainWindow::ClientRead(void)
{
    QString read = client ->readAll(); // Solo se puede leer una única vez, ya que una vez leido el valor de readAll se borra

    d -> EditPlainTextClient(QString("Read: ") + read + '\n');
}


void MainWindow::on_pushButtonSetting_clicked()
{
    d->open();
}


void MainWindow::LabelInformationServerClient(QString mode)
{
    if(mode == "server")
        ui -> labelInformationServerClient -> setText("Modo: SERVIDOR");
    else if (mode == "client")
        ui -> labelInformationServerClient -> setText("Modo: CLIENTE");
    else if (mode == "nothing")
        ui -> labelInformationServerClient -> setText("Modo: sin conexion");

}

void MainWindow::EnableDisableMainMenu(bool state)
{
    ui -> SliderX -> setEnabled(state);
    ui -> SliderY -> setEnabled(state);
    ui -> pushButtonArmed -> setEnabled(state);
    ui -> pushButtonShoot -> setEnabled(state);

}

void MainWindow::setSliderX(int value)
{
    ui->SliderX -> setValue(value);
}

void MainWindow::setSliderY(int value)
{
    ui->SliderY -> setValue(value);
}


void MainWindow::closeEvent (QCloseEvent *event)
{
    event -> accept(); // Se acepta el evento
    d->close(); // Se cierra la ventana "Configuracion IP"
}


/////////////////////////////////// PUERTO SERIE ////////////////////////////////////////////

void MainWindow::SerialPortWrite(QString data)
{
    request = data; //Se guarda en una variable privada a la que accedera transaction()
    transaction(); //Se hace efectiva la transmision
}

void MainWindow::on_ButtonSerialPort_clicked()
{

    MiPuertoSerie.show();
    MiPuertoSerie.exec();

}



void MainWindow::SettingSerialPort() { // Is SLOT

  port = MiPuertoSerie.getSerialPort();
  timeout = MiPuertoSerie.getWaitTimeOut();
  ui -> LabelPort -> setText(port);
  ui -> LabelTime-> setNum(timeout);

}

void MainWindow::on_SendButton_clicked()
{
    if(port.isEmpty())
    {
        QMessageBox::information(this,"COM","Puerto COM no detectado");
        return;
    }

    request = ui -> EditStringData -> text();
    transaction();
}


//    Methods for communicating through serial port

void MainWindow::transaction()
{
   // setControlsEnabled(false);
    ui->statusBar->showMessage(tr("Status: Running, connected to port %1.")
                         .arg(port));
    thread -> transaction(port,
                       timeout,
                       request);
}

void MainWindow::showResponse(const QString &s)
{
  //  setControlsEnabled(true);
    ui->statusBar->showMessage(tr("Traffic, transaction #%1:"
                             "\n\r-request: %2"
                             "\n\r-response: %3")
                          .arg(++transactionCount).arg(ui-> statusBar->currentMessage()).arg(s));
}

void MainWindow::processError(const QString &s)
{
   // setControlsEnabled(true);
    ui -> statusBar->showMessage(tr("Status: Not running, %1.").arg(s));
    ui -> statusBar -> showMessage(tr("No traffic."));
}

void MainWindow::processTimeout(const QString &s)
{
   // setControlsEnabled(true);
    ui ->statusBar->showMessage(tr("Status: Running, %1.").arg(s));
   // trafficLabel->setText(tr("No traffic."));
}


void MainWindow::on_EnableSliderButton_clicked() // Is a toggle button
{
    if(port.isEmpty())
    {
        QMessageBox::information(this,"COM","Puerto COM no detectado");
        return;
    }

    if(toggle == 1){

        ui-> pushButtonSetting -> setEnabled(0); //Se inhabilita el contro remoto
        ui->SliderX->setEnabled(true);
        ui->SliderY->setEnabled(true);
        ui->pushButtonArmed -> setEnabled(true);
        ui->pushButtonShoot -> setEnabled(true);
        ui->LabelInfo->show();
        setKeyboardEnable(1);
        toggle = 0;
    }
    else{
        ui-> pushButtonSetting -> setEnabled(1); //Se habilita el contro remoto
        ui->SliderX->setEnabled(false);
        ui->SliderY->setEnabled(false);
        ui->pushButtonArmed->setEnabled(false);
        ui->pushButtonShoot->setEnabled(false);
        ui->LabelInfo->hide();
        setKeyboardEnable(0);
        toggle = 1;
    }

}

void MainWindow::EnableDisableSliderButton(bool state)
{
    ui -> EnableSliderButton -> setEnabled(state);
}


bool MainWindow::getEnableDisableSliderButton()
{
    return toggle;
}


void MainWindow::on_SliderX_valueChanged(int)
{
   //Requests always has form "XnumerYnumber". Example: X200Y100 Sets X in 200 position and Y in 100 position

    int x = ui->SliderX-> value();
    int y = ui->SliderY-> value();

    ui-> DisplayX -> display(x); //Set LCDNumber
    /*
    //request = QString::number(n); // Store value in "request"
    request = QString("X%1Y%2\n").arg(x).arg(y);
    transaction();
    */

    SerialPortWrite(QString("X%1Y%2\n").arg(x).arg(y)); //Escribe por el puerto serie
}


void MainWindow::on_SliderY_valueChanged(int)
{

    int y = ui->SliderY-> value();
    int x = ui->SliderX ->value();

    ui-> DisplayY -> display(y); //Set LCDNumber

    /*
    //request = QString::number(n); // Store value in "request"
    request = QString("X%1Y%2\n").arg(x).arg(y);
    transaction();
    */

    SerialPortWrite(QString("X%1Y%2\n").arg(x).arg(y));
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{

    if(getKeyboardEnable()) //Activate keyboard if button "Activar" is enable

    {

        if(event -> key() == Qt::Key_A)
        {
            int x = ui -> SliderX -> value();
            if(x>=0)
                ui->SliderX->setValue(x-5);
        }


        if(event -> key() == Qt::Key_D)
        {
            int x = ui -> SliderX -> value();
            if(x<=255)
                ui->SliderX->setValue(x+5);
        }


        if(event -> key() == Qt::Key_W)
        {
            int y = ui -> SliderY -> value();
            if(y<=255)
                ui->SliderY->setValue(y+5);
        }


        if(event -> key() == Qt::Key_S)
        {
            int y = ui -> SliderY -> value();
            if(y>=0)
                ui->SliderY->setValue(y-5);
        }

    }
}

bool MainWindow::getKeyboardEnable()
{
    return KeyboardState;
}

void MainWindow::setKeyboardEnable(bool state)
{
    KeyboardState = state;
}


////////////////////////////// VISION ARTIFICIAL //////////////////////////////////////



void MainWindow::processFrameAndUpdateGUI()
{
    capwebcam.read(img);

    //cv::cvtColor(matOrg, matOrg,CV_BGR2RGB);
    cv::cvtColor(img, hsv, CV_BGR2HSV);

    // El valor que determina el color es el primer argumento, en este caso es azul 75-130
    cv::inRange(hsv, cv::Scalar(75, 50, 50), cv::Scalar(130, 255, 255), binary);

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15));
    cv::erode(binary, binary, element);
    cv::dilate(binary, binary, element);

    std::vector< std::vector<cv::Point> > contours;
    findContours(binary, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    drawContours(binary, contours, -1, cv::Scalar(255), CV_FILLED);


    for (std::vector<cv::Point> contour : contours) {


        // dibujar rectangulo
        cv::Rect r = boundingRect(contour);

        if(r.width >= MIN_OBJECT_X && r.height >= MIN_OBJECT_Y) //FILTRO: Solo se mostrará el rectangulo y se enviará infomracion si es mas grande que 100x100 pixeles
        {
                rectangle(img, r, CV_RGB(255, 0, 0), 2, CV_AA, 0);

                cv::Point center(r.x + (r.width / 2), r.y + (r.height / 2));

                std::ostringstream str;
                str << center.x << "," << center.y;

                position =  "X" + QString::number(center.x) + " Y" + QString::number(center.y) + "\n";
                EditPlainTextCamera(position); // Escribe por el Text Edit y despues se envia por BT


                // mostrar texto (x, y) Se obtienen las coordenadas en pixeles del punto medio del cuadrado rojo creado respecto de la pantalla 640x480px
                cv::putText(img, str.str(), center, cv::FONT_HERSHEY_COMPLEX_SMALL, 0.60, CV_RGB(0, 255, 0), 1, CV_AA);
        }
     }


    cv::cvtColor(img, img, CV_BGR2RGB); //Se pasa de BGR (que es como lee openCV a RGB (que es como funciona QImage)
    imageNormal=QImage((uchar*)img.data, img.cols, img.rows, img.step, QImage::Format_RGB888);
    imageBinary=QImage((uchar*)binary.data, binary.cols, binary.rows, binary.step, QImage::Format_Indexed8);

    //Se muestra por pantalla los dos vídeos
    ui -> LabelCapture -> setPixmap(QPixmap::fromImage(imageNormal));

    //Para ver la imagen en binario crear un LabelText con:
    // Nombre: LabelBinary  autoFillBackground: activado   frameShape: box
    //ui -> LabelBinary -> setPixmap(QPixmap::fromImage(imageBinary));

}




void MainWindow::EditPlainTextCamera(QString position){

     ui->plainTextEditCamera->insertPlainText(position);

     // Autoscroll
     ui -> plainTextEditCamera -> moveCursor(QTextCursor::End);
     ui -> plainTextEditCamera -> ensureCursorVisible();
}



void MainWindow::OpenCloseCamera()
{
    QString text = ui -> labelInformationServerClient -> text(); // El label se usaba para el modo servidor/cliente, pero se usara para todos los modos. Se cambiará

    //SE ENCEINDE EL SISTEMA
    if(text == "Modo: Sin conexión")
    {
        capwebcam.open(0);
        if (!capwebcam.isOpened()) //Si no hay camara o no se detecta, NO ENCIENDE el sistema
        {
            QMessageBox::information(this,"CamInfo","Cámara no detectada");
            return;
        }

        ui -> labelInformationServerClient -> setText("Modo: Automatico");

        //Se carga la imagen de la mirilla
        QPixmap pixmap( ":plantilla-mirilla-real.png" );
        ui -> LabelCaptureMask -> setPixmap(pixmap);
        ui -> LabelCaptureMask->setAlignment(Qt::AlignCenter);

        //Se enciende la camara
        frameTimer->start(30);

        capwebcam.set(CV_CAP_PROP_FRAME_WIDTH, CAMERA_WIDHT);
        capwebcam.set(CV_CAP_PROP_FRAME_HEIGHT,CAMERA_HEIGHT);
    }

    //SE APAGA EL SISTEMA
    if(text == "Modo: Automatico")
    {
        //Se cambia a modo sin conexion
        ui -> labelInformationServerClient -> setText("Modo: Sin conexión");

        //Se sustituye la imagen por un icono
        QPixmap pixmap( ":digital-camera-icon.png" );
        ui -> LabelCapture->setAlignment(Qt::AlignCenter);
        ui -> LabelCapture -> setPixmap(pixmap);
        ui -> LabelCaptureMask -> clear(); //Se hace transparente la mascara

        //Para el timer
        frameTimer -> stop();

        //Y si la camara estaba encendida, la apaga
        if (capwebcam.isOpened())
            {
                capwebcam.release();
            }

    }

}

void MainWindow::CameraToSerialPort()
{
    SerialPortWrite(position);
}


