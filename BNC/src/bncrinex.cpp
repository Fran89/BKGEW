// Part of BNC, a utility for retrieving decoding and
// converting GNSS data streams from NTRIP broadcasters.
//
// Copyright (C) 2007
// German Federal Agency for Cartography and Geodesy (BKG)
// http://www.bkg.bund.de
// Czech Technical University Prague, Department of Geodesy
// http://www.fsv.cvut.cz
//
// Email: euref-ip@bkg.bund.de
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

/* -------------------------------------------------------------------------
 * BKG NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      bncRinex
 *
 * Purpose:    writes RINEX files
 *
 * Author:     L. Mervart
 *
 * Created:    27-Aug-2006
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <sstream>

#include <QtCore>
#include <QUrl>
#include <QString>

#include "bncrinex.h"
#include "bnccore.h"
#include "bncutils.h"
#include "bncconst.h"
#include "bnctabledlg.h"
#include "bncgetthread.h"
#include "bncnetqueryv1.h"
#include "bncnetqueryv2.h"
#include "bncsettings.h"
#include "bncversion.h"

using namespace std;

// Constructor
////////////////////////////////////////////////////////////////////////////
bncRinex::bncRinex(const QByteArray& statID, const QUrl& mountPoint, 
                   const QByteArray& latitude, const QByteArray& longitude, 
                   const QByteArray& nmea, const QByteArray& ntripVersion) {

  _statID        = statID;
  _mountPoint    = mountPoint;
  _latitude      = latitude;
  _longitude     = longitude;
  _nmea          = nmea;
  _ntripVersion  = ntripVersion;
  _headerWritten = false;
  _reconnectFlag = false;

  bncSettings settings;
  _rnxScriptName = settings.value("rnxScript").toString();
  expandEnvVar(_rnxScriptName);

  _pgmName  = QString(BNCPGMNAME).leftJustified(20, ' ', true);
#ifdef WIN32
  _userName = QString("${USERNAME}");
#else
  _userName = QString("${USER}");
#endif
  expandEnvVar(_userName);
  _userName = _userName.leftJustified(20, ' ', true);

  _samplingRate = settings.value("rnxSampl").toInt();
}

// Destructor
////////////////////////////////////////////////////////////////////////////
bncRinex::~bncRinex() {
  bncSettings settings;
  if ((_header._version >= 3.0) && ( Qt::CheckState(settings.value("rnxAppend").toInt()) != Qt::Checked) ) {
    _out << ">                              4  1" << endl;
    _out << "END OF FILE" << endl;
  }
  _out.close();
}

// Download Skeleton Header File
////////////////////////////////////////////////////////////////////////////
t_irc bncRinex::downloadSkeleton() {

  t_irc irc = failure;

  QStringList table;
  bncTableDlg::getFullTable(_ntripVersion, _mountPoint.host(), 
                            _mountPoint.port(), table, true);
  QString net;
  QStringListIterator it(table);
  while (it.hasNext()) {
    QString line = it.next();
    if (line.indexOf("STR") == 0) {
      QStringList tags = line.split(";");
      if (tags.size() > 7) {
        if (tags.at(1) == _mountPoint.path().mid(1).toAscii()) {
          net = tags.at(7);
          break;
        }
      }
    }
  }
  QString sklDir;
  if (!net.isEmpty()) {
    it.toFront();
    while (it.hasNext()) {
      QString line = it.next();
      if (line.indexOf("NET") == 0) {
        QStringList tags = line.split(";");
        if (tags.size() > 6) {
          if (tags.at(1) == net) {
            sklDir = tags.at(6).trimmed();
            break;
          }
        }          
      }
    }
  }
  if (!sklDir.isEmpty() && sklDir != "none") {
    QUrl url(sklDir + "/" + _mountPoint.path().mid(1,4).toLower() + ".skl"); 
    if (url.port() == -1) {
      url.setPort(80);
    }

    bncNetQuery* query;
    if      (_ntripVersion == "2s") {
      query = new bncNetQueryV2(true);
    }
    else if (_ntripVersion == "2") {
      query = new bncNetQueryV2(false);
    }
    else {
      query = new bncNetQueryV1;
    }

    QByteArray outData;
    query->waitForRequestResult(url, outData);
    if (query->status() == bncNetQuery::finished) {
      irc = success;
      QTextStream in(outData);
      _sklHeader.read(&in);
    }

    delete query;
  }

  return irc;
}

// Read Skeleton Header File
////////////////////////////////////////////////////////////////////////////
bool bncRinex::readSkeleton() {

  bool readDone = false;

  // Read the local file
  // -------------------
  QFile skl(_sklName);
  if ( skl.exists() && skl.open(QIODevice::ReadOnly) ) {
    readDone = true;
    QTextStream in(&skl);
    _sklHeader.read(&in);
  }

  // Read downloaded file
  // --------------------
  else if ( _ntripVersion != "N" && _ntripVersion != "UN" &&
            _ntripVersion != "S" ) {
    QDate currDate = currentDateAndTimeGPS().date();
    if ( !_skeletonDate.isValid() || _skeletonDate != currDate ) {
      if (downloadSkeleton() == success) {
        readDone = true;
      }
      _skeletonDate = currDate;
    }
  }

  return readDone;
}

// Next File Epoch (static)
////////////////////////////////////////////////////////////////////////////
QString bncRinex::nextEpochStr(const QDateTime& datTim, 
                               const QString& intStr, QDateTime* nextEpoch) {

  QString epoStr;

  QTime nextTime;
  QDate nextDate;

  int indHlp = intStr.indexOf("min");

  if ( indHlp != -1) {
    int step = intStr.left(indHlp-1).toInt();
    char ch = 'A' + datTim.time().hour();
    epoStr = ch;
    if (datTim.time().minute() >= 60-step) {
      epoStr += QString("%1").arg(60-step, 2, 10, QChar('0'));
      if (datTim.time().hour() < 23) {
        nextTime.setHMS(datTim.time().hour() + 1 , 0, 0);
        nextDate = datTim.date();
      }
      else {
        nextTime.setHMS(0, 0, 0);
        nextDate = datTim.date().addDays(1);
      }
    }
    else {
      for (int limit = step; limit <= 60-step; limit += step) {
        if (datTim.time().minute() < limit) {
          epoStr += QString("%1").arg(limit-step, 2, 10, QChar('0'));
          nextTime.setHMS(datTim.time().hour(), limit, 0);
          nextDate = datTim.date();
          break;
        }
      }
    }
  }
  else if (intStr == "1 hour") {
    char ch = 'A' + datTim.time().hour();
    epoStr = ch;
    if (datTim.time().hour() < 23) {
      nextTime.setHMS(datTim.time().hour() + 1 , 0, 0);
      nextDate = datTim.date();
    }
    else {
      nextTime.setHMS(0, 0, 0);
      nextDate = datTim.date().addDays(1);
    }
  }
  else {
    epoStr = "0";
    nextTime.setHMS(0, 0, 0);
    nextDate = datTim.date().addDays(1);
  }

  if (nextEpoch) {
   *nextEpoch = QDateTime(nextDate, nextTime);
  }

  return epoStr;
}

// File Name according to RINEX Standards
////////////////////////////////////////////////////////////////////////////
void bncRinex::resolveFileName(const QDateTime& datTim) {

  bncSettings settings;
  QString path = settings.value("rnxPath").toString();
  expandEnvVar(path);

  if ( path.length() > 0 && path[path.length()-1] != QDir::separator() ) {
    path += QDir::separator();
  }

  QString hlpStr = nextEpochStr(datTim, settings.value("rnxIntr").toString(), 
                                &_nextCloseEpoch);

  QString ID4 = _statID.left(4);

  // Check name conflict
  // -------------------
  QString distStr;
  int num = 0;
  QListIterator<QString> it(settings.value("mountPoints").toStringList());
  while (it.hasNext()) {
    QString mp = it.next();
    if (mp.indexOf(ID4) != -1) {
      ++num;
    }
  }
  if (num > 1) {
    distStr = "_" + _statID.mid(4);
  }

  QString sklExt = settings.value("rnxSkel").toString();
  if (!sklExt.isEmpty()) {
    _sklName = path + ID4 + distStr + "." + sklExt;
  }

  path += ID4 +
          QString("%1").arg(datTim.date().dayOfYear(), 3, 10, QChar('0')) +
          hlpStr + distStr + datTim.toString(".yyO");

  _fName = path.toAscii();
}

// Write RINEX Header
////////////////////////////////////////////////////////////////////////////
void bncRinex::writeHeader(const QByteArray& format, 
                           const QDateTime& datTimNom) {

  bncSettings settings;

  // Open the Output File
  // --------------------
  resolveFileName(datTimNom);

  // Append to existing file and return
  // ----------------------------------
  if ( QFile::exists(_fName) &&
       (_reconnectFlag || Qt::CheckState(settings.value("rnxAppend").toInt()) == Qt::Checked) ) {
    _out.open(_fName.data(), ios::app);
    _out.setf(ios::showpoint | ios::fixed);
    _headerWritten = true;
    _reconnectFlag = false;
  }
  else {
    _out.open(_fName.data());
  }

  _out.setf(ios::showpoint | ios::fixed);

  // Set RINEX Version
  // -----------------
  int intHeaderVers = (Qt::CheckState(settings.value("rnxV3").toInt()) == Qt::Checked ? 3 : 2);

  // Read Skeleton Header
  // --------------------
  if (readSkeleton()) {
    _header.set(_sklHeader, intHeaderVers);
  }
  else {
    _header.setDefault(_statID, intHeaderVers);
  }

  // A Few Additional Comments
  // -------------------------
  _addComments << format.left(6) + " " + _mountPoint.host() + _mountPoint.path();
  if (_nmea == "yes") {
    _addComments << "NMEA LAT=" + _latitude + " " + "LONG=" + _longitude;
  }

  // Write the Header
  // ----------------
  QByteArray headerLines;
  QTextStream outHlp(&headerLines);

  QMap<QString, QString> txtMap;
  txtMap["COMMENT"] = _addComments.join("\\n");

  _header.write(&outHlp, &txtMap);

  outHlp.flush();

  if (!_headerWritten) {
    _out << headerLines.data();
  }

  _headerWritten = true;
}

// Stores Observation into Internal Array
////////////////////////////////////////////////////////////////////////////
void bncRinex::deepCopy(t_satObs obs) {
  _obs.push_back(obs);
}

// Write One Epoch into the RINEX File
////////////////////////////////////////////////////////////////////////////
void bncRinex::dumpEpoch(const QByteArray& format, long maxTime) {

  // Select observations older than maxTime
  // --------------------------------------
  QList<t_satObs> dumpList;
  QMutableListIterator<t_satObs> mIt(_obs);
  while (mIt.hasNext()) {
    t_satObs obs = mIt.next();
    if (obs._time.gpsw() * 7*24*3600 + obs._time.gpssec() < maxTime - 0.05) {
      dumpList.push_back(obs);
      mIt.remove();
    }
  }

  // Easy Return
  // -----------
  if (dumpList.isEmpty()) {
    return;
  }

  // Time of Epoch
  // -------------
  const t_satObs& fObs   = dumpList.first();
  QDateTime datTim    = dateAndTimeFromGPSweek(fObs._time.gpsw(), fObs._time.gpssec());
  QDateTime datTimNom = dateAndTimeFromGPSweek(fObs._time.gpsw(), 
                                               floor(fObs._time.gpssec()+0.5));

  // Close the file
  // --------------
  if (_nextCloseEpoch.isValid() && datTimNom >= _nextCloseEpoch) {
    closeFile();
    _headerWritten = false;
  }

  // Write RINEX Header
  // ------------------
  if (!_headerWritten) {
    _header._startTime.set(fObs._time.gpsw(), fObs._time.gpssec());
    writeHeader(format, datTimNom);
  }

  // Check whether observation types available
  // -----------------------------------------
  QMutableListIterator<t_satObs> mItDump(dumpList);
  while (mItDump.hasNext()) {
    t_satObs& obs = mItDump.next();
    if (!_header._obsTypes.contains(obs._prn.system()) && !_header._obsTypes.contains(obs._prn.system())) {
      mItDump.remove();
    }
  }
  if (dumpList.isEmpty()) {
    return;
  }

  double sec = double(datTim.time().second()) + fmod(fObs._time.gpssec(),1.0);

  // Epoch header line: RINEX Version 3
  // ----------------------------------
  if (_header._version >= 3.0) {
    _out << datTim.toString("> yyyy MM dd hh mm ").toAscii().data()
         << setw(10) << setprecision(7) << sec
         << "  " << 0 << setw(3)  << dumpList.size() << endl;
  }
  // Epoch header line: RINEX Version 2
  // ----------------------------------
  else {
    _out << datTim.toString(" yy MM dd hh mm ").toAscii().data()
         << setw(10) << setprecision(7) << sec
         << "  " << 0 << setw(3)  << dumpList.size();

    QListIterator<t_satObs> it(dumpList); int iSat = 0;
    while (it.hasNext()) {
      iSat++;
      const t_satObs& obs = it.next();
      _out << obs._prn.toString();
      if (iSat == 12 && it.hasNext()) {
        _out << endl << "                                ";
        iSat = 0;
      }
    }
    _out << endl;
  }

  QListIterator<t_satObs> it(dumpList);
  while (it.hasNext()) {
    const t_satObs& obs = it.next();

    // Cycle slips detection
    // ---------------------
    QString prn(obs._prn.toString().c_str());
    char  lli1 = ' ';
    char  lli2 = ' ';
    char  lli5 = ' ';
    char* lli = 0;

    for (unsigned iFrq = 0; iFrq < obs._obs.size(); iFrq++) {
      const t_frqObs*     frqObs   = obs._obs[iFrq];
      QMap<QString, int>* slip_cnt = 0;
      if      (frqObs->_rnxType2ch[0] == '1') {
        slip_cnt  = &_slip_cnt_L1;
        lli       = &lli1;
      }
      else if (frqObs->_rnxType2ch[0] == '2') {
        slip_cnt  = &_slip_cnt_L2;
        lli       = &lli2;
      }
      else if (frqObs->_rnxType2ch[0] == '5') {
        slip_cnt  = &_slip_cnt_L5;
        lli       = &lli5;
      }
      else {
        continue;
      }
      if (frqObs->_slip) {
        if ( slip_cnt->find(prn)         != slip_cnt->end() && 
             slip_cnt->find(prn).value() != frqObs->_slipCounter ) {
          *lli = '1';
        }
      }
      (*slip_cnt)[prn] = frqObs->_slipCounter;
    }

    // Write the data
    // --------------
    _out << rinexSatLine(obs, lli1, lli2, lli5) << endl;
  }

  _out.flush();
}

// Close the Old RINEX File
////////////////////////////////////////////////////////////////////////////
void bncRinex::closeFile() {
  if (_header._version == 3) {
    _out << ">                              4  1" << endl;
    _out << "END OF FILE" << endl;
  }
  _out.close();
  if (!_rnxScriptName.isEmpty()) {
    qApp->thread()->wait(100);
#ifdef WIN32
    QProcess::startDetached(_rnxScriptName, QStringList() << _fName) ;
#else
    QProcess::startDetached("nohup", QStringList() << _rnxScriptName << _fName) ;
#endif

  }
}

// One Line in RINEX v2 or v3
////////////////////////////////////////////////////////////////////////////
string bncRinex::rinexSatLine(const t_satObs& obs, char lli1, char lli2, char lli5) {
  ostringstream str;
  str.setf(ios::showpoint | ios::fixed);

  if (_header._version >= 3.0) {
    str << obs._prn.toString();
  }

  const QString obsKinds = "LCDS";

  char sys = obs._prn.system();
  const QVector<QString>& types = _header._obsTypes[sys];
  for (int ii = 0; ii < types.size(); ii++) {
    if (_header._version < 3.0 && ii > 0 && ii % 5 == 0) {
      str << endl;
    }
    double  obsValue = 0.0;
    char    lli      = ' ';
    QString rnxType = t_rnxObsFile::type2to3(sys, types[ii]);
    for (unsigned iFrq = 0; iFrq < obs._obs.size(); iFrq++) {
      const t_frqObs* frqObs = obs._obs[iFrq];
      for (int ik = 0; ik < obsKinds.length(); ik++) {
        QChar ch = obsKinds[ik];
        QString obsType = (ch + QString(frqObs->_rnxType2ch.c_str()));
        obsType = t_rnxObsFile::type2to3(sys, obsType).left(rnxType.length());
        if (rnxType == obsType) {
          if      (ch == 'L' && frqObs->_phaseValid) {
            obsValue = frqObs->_phase;
            if      (obsType[1] == '1') {
              lli = lli1;
            }
            else if (obsType[1] == '2') {
              lli = lli2;
            }
            else if (obsType[1] == '5') {
              lli = lli5;
            }
          }
          else if (ch == 'C' && frqObs->_codeValid) {
            obsValue = frqObs->_code;
          }
          else if (ch == 'D' && frqObs->_dopplerValid) {
            obsValue = frqObs->_doppler;
          }
          else if (ch == 'S' && frqObs->_snrValid) {
            obsValue = frqObs->_snr;
          }
        }
      }
    }
    str << setw(14) << setprecision(3) << obsValue << lli << ' ';
  }

  return str.str();
}

// 
////////////////////////////////////////////////////////////////////////////
string bncRinex::obsToStr(double val, int width, int precision) {
  if (val != 0.0) {
    ostringstream str;
    str.setf(ios::showpoint | ios::fixed);
    str << setw(width) << setprecision(precision) << val;
    return str.str();
  }
  else {
    return "0.0";
  }
}

// One Line in ASCII (Internal) Format
////////////////////////////////////////////////////////////////////////////
string bncRinex::asciiSatLine(const t_satObs& obs) {

  ostringstream str;
  str.setf(ios::showpoint | ios::fixed);

  str << obs._prn.toString() << ' ';
 
  for (unsigned ii = 0; ii < obs._obs.size(); ii++) {
    const t_frqObs* frqObs = obs._obs[ii];
    if (frqObs->_codeValid) {
      str << " C" << frqObs->_rnxType2ch << ' ' 
          << setw(14) << setprecision(3) << frqObs->_code;
    }
    if (frqObs->_phaseValid) {
      str << " L" << frqObs->_rnxType2ch << ' ' 
          << setw(14) << setprecision(3) << frqObs->_phase
          << ' ' << setw(3)  << frqObs->_slipCounter;
    }
    if (frqObs->_dopplerValid) {
      str << " D" << frqObs->_rnxType2ch << ' ' 
          << setw(14) << setprecision(3) << frqObs->_doppler;
    }
    if (frqObs->_snrValid) {
      str << " S" << frqObs->_rnxType2ch << ' ' 
          << setw(8) << setprecision(3) << frqObs->_snr;
    }
  }

  return str.str();
}
