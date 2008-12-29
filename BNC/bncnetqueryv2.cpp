/* -------------------------------------------------------------------------
 * BKG NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      bncNetQueryV2
 *
 * Purpose:    Blocking Network Requests (NTRIP Version 2)
 *
 * Author:     L. Mervart
 *
 * Created:    27-Dec-2008
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>

#include "bncnetqueryv2.h"
#include "bncapp.h"

using namespace std;

#define BNCVERSION "1.7"

// Constructor
////////////////////////////////////////////////////////////////////////////
bncNetQueryV2::bncNetQueryV2() {
  _manager   = new QNetworkAccessManager(this);
  _reply     = 0;
  _eventLoop = new QEventLoop(this);

  _status    = init;
}

// Destructor
////////////////////////////////////////////////////////////////////////////
bncNetQueryV2::~bncNetQueryV2() {
  delete _eventLoop;
  delete _reply;
  delete _manager;
}

// Error
////////////////////////////////////////////////////////////////////////////
void bncNetQueryV2::slotError(QNetworkReply::NetworkError) {
  _status = error;
  emit newMessage(_reply->errorString().toAscii(), true);
  _eventLoop->quit();
}

void bncNetQueryV2::slotFinished() {
  if (_status != error) {
    _status = finished;
  }
}

// Start request, block till the next read (public)
////////////////////////////////////////////////////////////////////////////
void bncNetQueryV2::startRequest(const QUrl& url, const QByteArray& gga) {
  startRequest(url, gga, false);
}

// Start request
////////////////////////////////////////////////////////////////////////////
void bncNetQueryV2::startRequest(const QUrl& url, const QByteArray& gga,
                                 bool full) {

  _status = running;

  // Default scheme and path
  // -----------------------
  QUrl urlLoc(url);
  if (urlLoc.scheme().isEmpty()) {
    urlLoc.setScheme("http");
  }
  if (urlLoc.path().isEmpty()) {
    urlLoc.setPath("/");
  }

  // Network Request
  // ---------------
  QNetworkRequest request;
  request.setUrl(urlLoc);
  request.setRawHeader("Host"         , urlLoc.host().toAscii());
  request.setRawHeader("Ntrip-Version", "NTRIP/2.0");
  request.setRawHeader("User-Agent"   , "NTRIP BNC/1.7");
  if (!urlLoc.userName().isEmpty()) {
    request.setRawHeader("Authorization", "Basic " + 
           (urlLoc.userName() + ":" + urlLoc.password()).toAscii().toBase64());
  } 
  request.setRawHeader("Connection"   , "close");

  _reply = _manager->get(request);

  // Connect Signals
  // ---------------
  connect(_reply, SIGNAL(finished()), this, SLOT(slotFinished()));
  connect(_reply, SIGNAL(finished()), _eventLoop, SLOT(quit()));
  if (!full) {
    connect(_reply, SIGNAL(readyRead()), _eventLoop, SLOT(quit()));
  }
  connect(_reply, SIGNAL(error(QNetworkReply::NetworkError)),
          this, SLOT(slotError(QNetworkReply::NetworkError)));
}

// Start Request, wait for its completion
////////////////////////////////////////////////////////////////////////////
void bncNetQueryV2::waitForRequestResult(const QUrl& url, QByteArray& outData) {

  // Send Request
  // ------------
  startRequest(url, "", true);

  // Wait Loop
  // ---------
  _eventLoop->exec();

  // Copy Data and Return
  // --------------------
  outData = _reply->readAll();
}

// Wait for next data
////////////////////////////////////////////////////////////////////////////
void bncNetQueryV2::waitForReadyRead(QByteArray& outData) {

  // Wait Loop
  // ---------
  if (!_reply->bytesAvailable()) {
    _eventLoop->exec();
  }

  // Append Data
  // -----------
  outData.append(_reply->readAll());
}
