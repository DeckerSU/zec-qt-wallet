#include "connection.h"
#include "mainwindow.h"
#include "settings.h"
#include "ui_connection.h"
#include "rpc.h"

#include "precompiled.h"

using json = nlohmann::json;

ConnectionLoader::ConnectionLoader(MainWindow* main, RPC* rpc, QString _ac_name) {
    this->main = main;
    this->rpc  = rpc;
    this->ac_name = _ac_name.isEmpty() ? "" : _ac_name.toUpper();

    d = new QDialog(main);
    connD = new Ui_ConnectionDialog();
    connD->setupUi(d);
    QPixmap logo(":/img/res/logobig.gif");
    connD->topIcon->setBasePixmap(logo.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

ConnectionLoader::~ConnectionLoader() {    
    delete d;
    delete connD;
}


std::shared_ptr<ConnectionConfig> ConnectionLoader::getConnectionConfig() {

    auto config = autoDetectZcashConf(); // std::shared_ptr<ConnectionConfig>
     if (config.get() != nullptr) {
        return config;
     }
    return nullptr;
}

void ConnectionLoader::loadConnection() {
    QTimer::singleShot(1, [=]() { this->doAutoConnect(); });
    d->exec();
}

void ConnectionLoader::doAutoConnect(bool tryEzcashdStart) {
    // Priority 1: Ensure all params are present.
    if (!verifyParams()) {
        downloadParams([=]() { this->doAutoConnect(); });
        return;
    }

    // Priority 2: Try to connect to detect komodo.conf and connect to it.
    auto config = autoDetectZcashConf();

    // small logic change: if --no-embedded passed we shouldn't use rpc connection
    // data from exising .conf, instead of that we read settings from QSettings (!)
    // and do manual connection. Settings is stored in platform-specific locations:
    // http://doc.qt.io/qt-5/qsettings.html#platform-specific-notes . Also there is
    // a various formats http://doc.qt.io/qt-5/qsettings.html#Format-enum to store
    // settings .

    // For Windows 64-bit default location for Settings is system registry, default
    // key is HKEY_CURRENT_USER\Software\kmd-qt-wallet-org , so to delete all settings
    // you just need to do something like "reg delete HKEY_CURRENT_USER\Software\kmd-qt-wallet-org"

    if (Settings::getInstance()->useEmbedded() == false) {
         config = nullptr; // std::shared_ptr<ConnectionConfig>
         Settings::getInstance()->clearZcashdConfLocation();
    }

    main->logger->write(QObject::tr("Attempting autoconnect"));

    if (config.get() != nullptr) {
        auto connection = makeConnection(config);

        refreshZcashdState(connection, [=] () {
            // Refused connection. So try and start embedded komodod
            if (Settings::getInstance()->useEmbedded()) {
                if (tryEzcashdStart) {
                    this->showInformation(QObject::tr("Starting embedded komodod"));
                    if (this->startEmbeddedZcashd()) {
                        // Embedded komodod started up. Wait a second and then refresh the connection
                        main->logger->write("Embedded komodod started up, trying autoconnect in 1 sec");
                        QTimer::singleShot(1000, [=]() { doAutoConnect(); } );
                    } else {
                        if (config->zcashDaemon) {
                            // komodod is configured to run as a daemon, so we must wait for a few seconds
                            // to let it start up. 
                            main->logger->write("komodod is daemon=1. Waiting for it to start up");
                            this->showInformation(QObject::tr("komodod is set to run as daemon"), QObject::tr("Waiting for komodod"));
                            QTimer::singleShot(5000, [=]() { doAutoConnect(/* don't attempt to start ekomodod */ false); });
                        } else {
                            // Something is wrong. 
                            // We're going to attempt to connect to the one in the background one last time
                            // and see if that works, else throw an error
                            main->logger->write("Unknown problem while trying to start komodod");
                            QTimer::singleShot(2000, [=]() { doAutoConnect(/* don't attempt to start ekomodod */ false); });
                        }
                    }
                } else {
                    // We tried to start ekomodod previously, and it didn't work. So, show the error. 
                    main->logger->write("Couldn't start embedded komodod for unknown reason");
                    QString explanation;
                    if (config->zcashDaemon) {
                        explanation = QString() % QObject::tr("You have komodod set to start as a daemon, which can cause problems "
                            "with kmd-qt-wallet\n\n."
                            "Please remove the following line from your komodo.conf and restart kmd-qt-wallet\n"
                            "daemon=1");
                    } else {
                        explanation = QString() % QObject::tr("Couldn't start the embedded komodod.\n\n" 
                            "Please try restarting.\n\nIf you previously started komodod with custom arguments, you might need to reset komodo.conf.\n\n" 
                            "If all else fails, please run komodod manually.") %  
                            (ezcashd ? QObject::tr("The process returned") + ":\n\n" % ezcashd->errorString() : QString(""));
                    }
                    
                    this->showError(explanation);
                }                
            } else {
                // komodo.conf exists, there's no connection, and the user asked us not to start komodod. Error!
                main->logger->write("Not using embedded and couldn't connect to komodod");
                QString explanation = QString() % QObject::tr("Couldn't connect to komodod configured in komodo.conf.\n\n" 
                                      "Not starting embedded komodod because --no-embedded was passed");
                this->showError(explanation);
            }
        });
    } else {
        if (Settings::getInstance()->useEmbedded()) {
            // komodo.conf was not found, so create one
            createZcashConf();
        } else {
            // Fall back to manual connect
            doManualConnect();
        }
    } 
}

QString randomPassword() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    const int passwordLength = 10;
    char* s = new char[passwordLength + 1];

    for (int i = 0; i < passwordLength; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[passwordLength] = 0;
    return QString::fromStdString(s);
}

/**
 * This will create a new komodo.conf, download Zcash parameters.
 */ 
void ConnectionLoader::createZcashConf() {
    main->logger->write("createZcashConf");

    auto confLocation = zcashConfWritableLocation();
    main->logger->write("Creating file " + confLocation);

    QFileInfo fi(confLocation);
    QDir().mkdir(fi.dir().absolutePath());

    QFile file(confLocation);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        main->logger->write("Could not create komodo.conf, returning");
        return;
    }
        
    QTextStream out(&file); 
    
    out << "server=1\n";
    out << "txindex=1\n";
    out << "addnode=5.9.102.210\n";
    out << "addnode=78.47.196.146\n";
    out << "addnode=178.63.69.164\n";
    out << "addnode=88.198.65.74\n";
    out << "addnode=5.9.122.241\n";
    out << "addnode=144.76.94.38\n";
    out << "addnode=89.248.166.91\n";
    out << "rpcuser=kmd-qt-wallet\n";
    out << "rpcpassword=" % randomPassword() << "\n";
    file.close();

    // Now that komodo.conf exists, try to autoconnect again
    this->doAutoConnect();
}


void ConnectionLoader::downloadParams(std::function<void(void)> cb) {    
    main->logger->write("Adding params to download queue");
    // Add all the files to the download queue
    downloadQueue = new QQueue<QUrl>();
    client = new QNetworkAccessManager(main);   
    
    downloadQueue->enqueue(QUrl("https://z.cash/downloads/sapling-output.params"));
    downloadQueue->enqueue(QUrl("https://z.cash/downloads/sapling-spend.params"));    
    downloadQueue->enqueue(QUrl("https://z.cash/downloads/sprout-proving.key"));
    downloadQueue->enqueue(QUrl("https://z.cash/downloads/sprout-verifying.key"));
    downloadQueue->enqueue(QUrl("https://z.cash/downloads/sprout-groth16.params"));

    doNextDownload(cb);    
}

void ConnectionLoader::doNextDownload(std::function<void(void)> cb) {
    auto fnSaveFileName = [&] (QUrl url) {
        QString path = url.path();
        QString basename = QFileInfo(path).fileName();

        return basename;
    };

    if (downloadQueue->isEmpty()) {
        delete downloadQueue;
        client->deleteLater();

        main->logger->write("All Downloads done");
        this->showInformation(QObject::tr("All Downloads Finished Successfully!"));
        cb();
        return;
    }

    QUrl url = downloadQueue->dequeue();
    int filesRemaining = downloadQueue->size();

    QString filename = fnSaveFileName(url);
    QString paramsDir = zcashParamsDir();

    if (QFile(QDir(paramsDir).filePath(filename)).exists()) {
        main->logger->write(filename + " already exists, skipping");
        doNextDownload(cb);

        return;
    }

    // The downloaded file is written to a new name, and then renamed when the operation completes.
    currentOutput = new QFile(QDir(paramsDir).filePath(filename + ".part"));   

    if (!currentOutput->open(QIODevice::WriteOnly)) {
        main->logger->write("Couldn't open " + currentOutput->fileName() + " for writing");
        this->showError(QObject::tr("Couldn't download params. Please check the help site for more info."));
    }
    main->logger->write("Downloading to " + filename);
    qDebug() << "Downloading " << url << " to " << filename;
    
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    currentDownload = client->get(request);
    downloadTime.start();
    
    // Download Progress
    QObject::connect(currentDownload, &QNetworkReply::downloadProgress, [=] (auto done, auto total) {
        // calculate the download speed
        double speed = done * 1000.0 / downloadTime.elapsed();
        QString unit;
        if (speed < 1024) {
            unit = "bytes/sec";
        } else if (speed < 1024*1024) {
            speed /= 1024;
            unit = "kB/s";
        } else {
            speed /= 1024*1024;
            unit = "MB/s";
        }

        this->showInformation(
            QObject::tr("Downloading ") % filename % (filesRemaining > 1 ? " ( +" % QString::number(filesRemaining)  % QObject::tr(" more remaining )") : QString("")),
            QString::number(done/1024/1024, 'f', 0) % QObject::tr("MB of ") % QString::number(total/1024/1024, 'f', 0) + QObject::tr("MB at ") % QString::number(speed, 'f', 2) % unit);
    });
    
    // Download Finished
    QObject::connect(currentDownload, &QNetworkReply::finished, [=] () {
        // Rename file
        main->logger->write("Finished downloading " + filename);
        currentOutput->rename(QDir(paramsDir).filePath(filename));

        currentOutput->close();
        currentDownload->deleteLater();
        currentOutput->deleteLater();

        if (currentDownload->error()) {
            main->logger->write("Downloading " + filename + " failed");
            this->showError(QObject::tr("Downloading ") + filename + QObject::tr(" failed. Please check the help site for more info"));                
        } else {
            doNextDownload(cb);
        }
    });

    // Download new data available. 
    QObject::connect(currentDownload, &QNetworkReply::readyRead, [=] () {
        currentOutput->write(currentDownload->readAll());
    });    
}

bool ConnectionLoader::startEmbeddedZcashd() {
    if (!Settings::getInstance()->useEmbedded()) 
        return false;
    
    main->logger->write("Trying to start embedded komodod");

    // Static because it needs to survive even after this method returns.
    static QString processStdErrOutput;

    if (ezcashd != nullptr) {
        if (ezcashd->state() == QProcess::NotRunning) {
            if (!processStdErrOutput.isEmpty()) {
                QMessageBox::critical(main, QObject::tr("komodod error"), "komodod said: " + processStdErrOutput, 
                                      QMessageBox::Ok);
            }
            return false;
        } else {
            return true;
        }        
    }

    // Finally, start komodod    
    QDir appPath(QCoreApplication::applicationDirPath());
#ifdef Q_OS_LINUX
    //auto zcashdProgram = appPath.absoluteFilePath("zqw-komodod");
    //if (!QFile(zcashdProgram).exists()) {
    auto zcashdProgram = appPath.absoluteFilePath("komodod");
    //}
#elif defined(Q_OS_DARWIN)
    auto zcashdProgram = appPath.absoluteFilePath("komodod");
#else
    auto zcashdProgram = appPath.absoluteFilePath("komodod.exe");
#endif
    
    if (!QFile(zcashdProgram).exists()) {
        qDebug() << "Can't find komodod at " << zcashdProgram;
        main->logger->write("Can't find komodod at " + zcashdProgram); 
        return false;
    }

    ezcashd = new QProcess(main);    
    QObject::connect(ezcashd, &QProcess::started, [=] () {
        //qDebug() << "komodod started";
    });

    QObject::connect(ezcashd, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        [=](int, QProcess::ExitStatus) {
        //qDebug() << "komodod finished with code " << exitCode << "," << exitStatus;    
    });

    QObject::connect(ezcashd, &QProcess::errorOccurred, [&] (auto) {
        //qDebug() << "Couldn't start komodod: " << error;
    });

    QObject::connect(ezcashd, &QProcess::readyReadStandardError, [=]() {
        auto output = ezcashd->readAllStandardError();
       main->logger->write("komodod stderr:" + output);
        processStdErrOutput.append(output);
    });

#ifdef Q_OS_LINUX
    ezcashd->start(zcashdProgram);
#elif defined(Q_OS_DARWIN)
    ezcashd->start(zcashdProgram);
#else
    ezcashd->setWorkingDirectory(appPath.absolutePath());
    ezcashd->start("komodod.exe");
#endif // Q_OS_LINUX


    return true;
}

void ConnectionLoader::doManualConnect() {
    auto config = loadFromSettings();

    if (!config) {
        // Nothing configured, show an error
        QString explanation = QString()
                % QObject::tr("A manual connection was requested, but the settings are not configured.\n\n"
                "Please set the host/port and user/password in the Edit->Settings menu.");

        showError(explanation);
        doRPCSetConnection(nullptr);

        return;
    }

    auto connection = makeConnection(config);
    refreshZcashdState(connection, [=] () {
        QString explanation = QString()
                % QObject::tr("Could not connect to komodod configured in settings.\n\n" 
                "Please set the host/port and user/password in the Edit->Settings menu.");

        showError(explanation);
        doRPCSetConnection(nullptr);

        return;
    });
}

void ConnectionLoader::doRPCSetConnection(Connection* conn) {
    rpc->setEZcashd(ezcashd);
    rpc->setConnection(conn);
    
    d->accept();

    delete this;
}

Connection* ConnectionLoader::makeConnection(std::shared_ptr<ConnectionConfig> config) {
    QNetworkAccessManager* client = new QNetworkAccessManager(main);
         
    QUrl myurl;
    myurl.setScheme("http");
    myurl.setHost(config.get()->host);
    myurl.setPort(config.get()->port.toInt());

    QNetworkRequest* request = new QNetworkRequest();
    request->setUrl(myurl);
    request->setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
    
    QString userpass = config.get()->rpcuser % ":" % config.get()->rpcpassword;
    QString headerData = "Basic " + userpass.toLocal8Bit().toBase64();
    request->setRawHeader("Authorization", headerData.toLocal8Bit());    

    return new Connection(main, client, request, config);
}

void ConnectionLoader::refreshZcashdState(Connection* connection, std::function<void(void)> refused) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "getinfo"}
    };
    connection->doRPC(payload,
        [=] (auto) {
            // Success, hide the dialog if it was shown. 
            d->hide();
            this->doRPCSetConnection(connection);
        },
        [=] (auto reply, auto res) {            
            // Failed, see what it is. 
            auto err = reply->error();
            //qDebug() << err << ":" << QString::fromStdString(res.dump());

            if (err == QNetworkReply::NetworkError::ConnectionRefusedError) {   
                refused();
            } else if (err == QNetworkReply::NetworkError::AuthenticationRequiredError) {
                main->logger->write("Authentication failed");
                QString explanation = QString() % 
                        QObject::tr("Authentication failed. The username / password you specified was "
                        "not accepted by komodod. Try changing it in the Edit->Settings menu");

                this->showError(explanation);
            } else if (err == QNetworkReply::NetworkError::InternalServerError && 
                    !res.is_discarded()) {
                // The server is loading, so just poll until it succeeds
                QString status    = QString::fromStdString(res["error"]["message"]);
                {
                    static int dots = 0;
                    status = status.left(status.length() - 3) + QString(".").repeated(dots);
                    dots++;
                    if (dots > 3)
                        dots = 0;
                }
                this->showInformation(QObject::tr("Your komodod is starting up. Please wait."), status);
                main->logger->write("Waiting for komodod to come online.");
                // Refresh after one second
                QTimer::singleShot(1000, [=]() { this->refreshZcashdState(connection, refused); });
            }
        }
    );
}

void ConnectionLoader::showInformation(QString info, QString detail) {
    connD->status->setText(info);
    connD->statusDetail->setText(detail);
}

/**
 * Show error will close the loading dialog and show an error. 
*/
void ConnectionLoader::showError(QString explanation) {    
    rpc->setEZcashd(nullptr);
    rpc->noConnection();

    QMessageBox::critical(main, QObject::tr("Connection Error"), explanation, QMessageBox::Ok);
    d->close();
}

QString ConnectionLoader::locateZcashConfFile() {
bool isKMD = this->ac_name.isEmpty() || !QString::compare(this->ac_name, "KMD", Qt::CaseInsensitive);
#ifdef Q_OS_LINUX
    auto confLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, ".komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#elif defined(Q_OS_DARWIN)
    auto confLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, "Library/Application Support/Komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#else
    auto confLocation = QStandardPaths::locate(QStandardPaths::AppDataLocation, "../../Komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#endif

    main->logger->write("Found zcashconf at " + QDir::cleanPath(confLocation));
    return QDir::cleanPath(confLocation);
}

QString ConnectionLoader::zcashConfWritableLocation() {
bool isKMD = this->ac_name.isEmpty() || !QString::compare(this->ac_name, "KMD", Qt::CaseInsensitive);
#ifdef Q_OS_LINUX
    auto confLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath(".komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#elif defined(Q_OS_DARWIN)
    auto confLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath("Library/Application Support/Komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#else
    auto confLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("../../Komodo/" + (isKMD ? "komodo.conf" : this->ac_name + "/" + this->ac_name + ".conf"));
#endif

    main->logger->write("Found zcashconf at " + QDir::cleanPath(confLocation));
    return QDir::cleanPath(confLocation);
}

QString ConnectionLoader::zcashParamsDir() {
    #ifdef Q_OS_LINUX
    auto paramsLocation = QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath(".zcash-params"));
#elif defined(Q_OS_DARWIN)
    auto paramsLocation = QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath("Library/Application Support/ZcashParams"));
#else
    auto paramsLocation = QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("../../ZcashParams"));
#endif

    if (!paramsLocation.exists()) {
        main->logger->write("Creating params location at " + paramsLocation.absolutePath());
        QDir().mkpath(paramsLocation.absolutePath());
    }

    main->logger->write("Found Zcash params directory at " + paramsLocation.absolutePath());
    return paramsLocation.absolutePath();
}

bool ConnectionLoader::verifyParams() {
    QDir paramsDir(zcashParamsDir());

    if (!QFile(paramsDir.filePath("sapling-output.params")).exists()) return false;
    if (!QFile(paramsDir.filePath("sapling-spend.params")).exists()) return false;
    if (!QFile(paramsDir.filePath("sprout-proving.key")).exists()) return false;
    if (!QFile(paramsDir.filePath("sprout-verifying.key")).exists()) return false;
    if (!QFile(paramsDir.filePath("sprout-groth16.params")).exists()) return false;

    return true;
}

/**
 * Try to automatically detect a komodo.conf file in the correct location and load parameters
 */ 
std::shared_ptr<ConnectionConfig> ConnectionLoader::autoDetectZcashConf() {    
    auto confLocation = locateZcashConfFile();

    if (confLocation.isNull()) {
        // No Zcash file, just return with nothing
        return nullptr;
    }

    QFile file(confLocation);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << file.errorString();
        return nullptr;
    }

    QTextStream in(&file);

    auto zcashconf = new ConnectionConfig();
    zcashconf->host     = "127.0.0.1";
    zcashconf->connType = ConnectionType::DetectedConfExternalZcashD;
    zcashconf->usingZcashConf = true;
    zcashconf->zcashDir = QFileInfo(confLocation).absoluteDir().absolutePath();
    zcashconf->zcashDaemon = false;

    Settings::getInstance()->setUsingZcashConf(confLocation);

    while (!in.atEnd()) {
        QString line = in.readLine();
        auto s = line.indexOf("=");
        QString name  = line.left(s).trimmed().toLower();
        QString value = line.right(line.length() - s - 1).trimmed();

        if (name == "rpcuser") {
            zcashconf->rpcuser = value;
        }
        if (name == "rpcpassword") {
            zcashconf->rpcpassword = value;
        }
        if (name == "rpcport") {
            zcashconf->port = value;
        }
        if (name == "daemon" && value == "1") {
            zcashconf->zcashDaemon = true;
        }
        if (name == "proxy") {
            zcashconf->proxy = value;
        }
        if (name == "testnet" &&
            value == "1"  &&
            zcashconf->port.isEmpty()) {
                zcashconf->port = "17771";
        }
    }

    // If rpcport is not in the file, and it was not set by the testnet=1 flag, then go to default
    if (zcashconf->port.isEmpty()) zcashconf->port = "7771";
    file.close();

    // In addition to the komodo.conf file, also double check the params. 

    return std::shared_ptr<ConnectionConfig>(zcashconf);
}

/**
 * Load connection settings from the UI, which indicates an unknown, external komodod
 */ 
std::shared_ptr<ConnectionConfig> ConnectionLoader::loadFromSettings() {
    // Load from the QT Settings. 
    QSettings s;
    
    auto host        = s.value("connection/host").toString();
    auto port        = s.value("connection/port").toString();
    auto username    = s.value("connection/rpcuser").toString();
    auto password    = s.value("connection/rpcpassword").toString();    

    if (username.isEmpty() || password.isEmpty())
        return nullptr;

    auto uiConfig = new ConnectionConfig{ host, port, username, password, false, false, "", "", ConnectionType::UISettingsZCashD};

    return std::shared_ptr<ConnectionConfig>(uiConfig);
}





/***********************************************************************************
 *  Connection Class
 ************************************************************************************/ 
Connection::Connection(MainWindow* m, QNetworkAccessManager* c, QNetworkRequest* r, 
                        std::shared_ptr<ConnectionConfig> conf) {
    this->restclient  = c;
    this->request     = r;
    this->config      = conf;
    this->main        = m;
}

Connection::~Connection() {
    delete restclient;
    delete request;
}

void Connection::doRPC(const json& payload, const std::function<void(json)>& cb, 
                       const std::function<void(QNetworkReply*, const json&)>& ne) {
    if (shutdownInProgress) {
        // Ignoring RPC because shutdown in progress
        return;
    }

    QNetworkReply *reply = restclient->post(*request, QByteArray::fromStdString(payload.dump()));

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        reply->deleteLater();
        if (shutdownInProgress) {
            // Ignoring callback because shutdown in progress
            return;
        }
        
        if (reply->error() != QNetworkReply::NoError) {
            auto parsed = json::parse(reply->readAll(), nullptr, false);
            ne(reply, parsed);
            
            return;
        } 
        
        auto parsed = json::parse(reply->readAll(), nullptr, false);
        if (parsed.is_discarded()) {
            ne(reply, "Unknown error");
        }
        
        cb(parsed["result"]);        
    });
}

void Connection::doRPCWithDefaultErrorHandling(const json& payload, const std::function<void(json)>& cb) {
    doRPC(payload, cb, [=] (auto reply, auto parsed) {
        if (!parsed.is_discarded() && !parsed["error"]["message"].is_null()) {
            this->showTxError(QString::fromStdString(parsed["error"]["message"]));    
        } else {
            this->showTxError(reply->errorString());
        }
    });
}

void Connection::doRPCIgnoreError(const json& payload, const std::function<void(json)>& cb) {
    doRPC(payload, cb, [=] (auto, auto) {
        // Ignored error handling
    });
}

void Connection::showTxError(const QString& error) {
    if (error.isNull()) return;

    // Prevent multiple dialog boxes from showing, because they're all called async
    static bool shown = false;
    if (shown)
        return;

    shown = true;
    QMessageBox::critical(main, QObject::tr("Transaction Error"), QObject::tr("There was an error sending the transaction. The error was:") + "\n\n"
        + error, QMessageBox::StandardButton::Ok);
    shown = false;
}

/**
 * Prevent all future calls from going through
 */ 
void Connection::shutdown() {
    shutdownInProgress = true;
}
