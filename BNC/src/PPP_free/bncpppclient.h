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

#ifndef BNCPPPCLIENT_H
#define BNCPPPCLIENT_H

#include <queue>
#include "bncephuser.h"
#include "GPSDecoder.h"


namespace BNC_PPP {
  
class bncModel;
class t_pppOptions;

class t_satData {
 public:
  t_satData() {
    obsIndex = 0;
    P1 = 0.0;
    P2 = 0.0;
    P5 = 0.0;
    P3 = 0.0;
    L1 = 0.0;
    L2 = 0.0;
    L5 = 0.0;
    L3 = 0.0;
  }
  ~t_satData() {}
  bncTime      tt;
  QString      prn;
  double       P1;
  double       P2;
  double       P5;
  double       P3;
  double       L1;
  double       L2;
  double       L5;
  double       L3;
  ColumnVector xx;
  ColumnVector vv;
  double       clk;
  double       eleSat;
  double       azSat;
  double       rho;
  bool         slipFlag;
  double       lambda3;
  unsigned     obsIndex;
  char system() const {return prn.toAscii()[0];}
};

class t_epoData {
 public:
  t_epoData() {}

  ~t_epoData() {
    clear();
  }

  void clear() {
    QMapIterator<QString, t_satData*> it(satData);
    while (it.hasNext()) {
      it.next();
      delete it.value();
    }
    satData.clear();
  }

  void deepCopy(const t_epoData* from) {
    clear();
    tt = from->tt;
    QMapIterator<QString, t_satData*> it(from->satData);
    while (it.hasNext()) {
      it.next();
      satData[it.key()] = new t_satData(*it.value());
    }
  }

  unsigned sizeSys(char system) const {
    unsigned ans = 0;
    QMapIterator<QString, t_satData*> it(satData);
    while (it.hasNext()) {
      it.next();
      if (it.value()->system() == system) {
        ++ans;
      }
    }
    return ans;
  }
  unsigned sizeAll() const {return satData.size();}

  bncTime                   tt;
  QMap<QString, t_satData*> satData;
};

class bncPPPclient : public bncEphUser {
 Q_OBJECT

 public:
  bncPPPclient(QByteArray staID, t_pppOptions* opt = 0, bool connectSlots = true);
  ~bncPPPclient();
  void putNewObs(const t_obs& pp);
  static t_irc applyCorr(const bncTime& tt, const t_corr* cc, ColumnVector& xc, 
                         ColumnVector& vv);
  QByteArray staID() const {return _staID;}
  const t_pppOptions* opt() const {return _opt;}
  void emitNewMessage(QByteArray msg, bool showOnScreen) {
    emit newMessage(msg, showOnScreen);
  }
  void emitNewNMEAstr(QByteArray str) {
    emit newNMEAstr(str);
  }

 public slots:
  void slotNewCorrections(QList<QString> corrList);
  void slotProviderIDChanged(QString mountPoint);

 signals:
  void newMessage(QByteArray msg, bool showOnScreen);
  void newPosition(bncTime time, double x, double y, double z);
  void newNMEAstr(QByteArray str);

 private:
  class slipInfo {
   public:
    slipInfo() {
      slipCntL1 = -1;
      slipCntL2 = -1;
      slipCntL5 = -1;
    }
    ~slipInfo(){}
    int slipCntL1;
    int slipCntL2;
    int slipCntL5;
  };

  t_irc getSatPos(const bncTime& tt, const QString& prn, 
                  ColumnVector& xc, ColumnVector& vv);
  void processEpochs();
  void processFrontEpoch();
  t_irc cmpToT(t_satData* satData);

  t_pppOptions*           _opt;
  bool                    _optOwner;
  QByteArray              _staID;
  QMap<QString, t_corr*>  _corr;
  bncTime                 _corr_tt;
  QMap<QString, t_bias*>  _bias;
  std::queue<t_epoData*>  _epoData;
  bncModel*               _model;
  QMap<QString, slipInfo> _slips;
};

}

#endif