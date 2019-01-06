#ifndef SETTINGS_H
#define SETTINGS_H

#include "precompiled.h"

struct Config {
    QString host;
    QString port;
    QString rpcuser;
    QString rpcpassword;
    QString assetchain;
};

struct ToFields;
struct Tx;

class Settings
{
public:
    static  Settings* init();
    static  Settings* getInstance();

    Config  getSettings();
    void    saveSettings(const QString& host, const QString& port, const QString& username, const QString& password, const QString& assetchain);

    bool    isTestnet();
    void    setTestnet(bool isTestnet);
            
    bool    isSaplingAddress(QString addr);
    bool    isSproutAddress(QString addr);
            
    bool    isSyncing();
    void    setSyncing(bool syncing);

    void    setUseEmbedded(bool r) { _useEmbedded = r; }
    bool    useEmbedded() { return _useEmbedded; }

    int     getBlockNumber();
    void    setBlockNumber(int number);
            
    bool    getSaveZtxs();
    void    setSaveZtxs(bool save);

    bool    getAutoShield();
    void    setAutoShield(bool allow);

    bool    getAllowCustomFees();
    void    setAllowCustomFees(bool allow);
            
    bool    isSaplingActive();

    void    setUsingZcashConf(QString confLocation);
    //void    clearZcashdConfLocation();
    const   QString& getZcashdConfLocation() { return _confLocation; }

    void    setAssetChainName(QString token);
    const   QString& getAssetChainName() { return _assetchainName; }

    void    setZECPrice(double p) { zecPrice = p; }
    double  getZECPrice();

    void    setPeers(int peers);
    int     getPeers();
       
    // Static stuff
    static const QString txidStatusMessage;
    
    static void saveRestore(QDialog* d);

    static bool    isZAddress(QString addr);
    static bool    isTAddress(QString addr);

    static QString getDecimalString(double amt);
    static QString getUSDFormat(double bal);
    static QString getZECDisplayFormat(double bal);
    static QString getZECUSDDisplayFormat(double bal);

    static QString getTokenName();
    static QString getDonationAddr(bool sapling);

    static double  getMinerFee();
    static double  getZboardAmount();
    static QString getZboardAddr();
    
    static bool    isValidAddress(QString addr);

    static bool    addToZcashConf(QString confLocation, QString line);
    static bool    removeFromZcashConf(QString confLocation, QString option);

    static const QString labelRegExp;

    static const int     updateSpeed         = 20 * 1000;        // 20 sec
    static const int     quickUpdateSpeed    = 5  * 1000;        // 5 sec
    static const int     priceRefreshSpeed   = 60 * 60 * 1000;   // 1 hr

private:
    // This class can only be accessed through Settings::getInstance()
    Settings() = default;
    ~Settings() = default;

    static Settings* instance;

    QString _confLocation;
    QString _executable;
    bool    _isTestnet        = false;
    bool    _isSyncing        = false;
    int     _blockNumber      = 0;
    bool    _useEmbedded      = false;
    int     _peerConnections  = 0;
    QString _assetchainName = "";

    double zecPrice = 0.0;
};

#endif // SETTINGS_H
