#ifndef BNCRTROVER_H
#define BNCRTROVER_H

#include <vector>

#include "RTCM/GPSDecoder.h"
#include "bncephuser.h"

class t_bncRtrover : public QThread {
 Q_OBJECT

 public:
  t_bncRtrover();
  ~t_bncRtrover();
  virtual void run();

 public slots:
  void slotNewEphGPS(gpsephemeris gpseph);
  void slotNewEphGlonass(glonassephemeris gloeph);
  void slotNewEphGalileo(galileoephemeris galeph);
  void slotNewCorrections(QList<QString> corrList);
  void slotNewObs(QByteArray staID, QList<t_obs> obsList);

 private:
  class t_epoData {
   public:
    bncTime            _time;
    std::vector<t_obs> _obsRover; 
    std::vector<t_obs> _obsBase; 
  };
  void processFrontEpoch();
  QMutex                  _mutex;
  QByteArray              _mode;
  QFile                   _outputFile;
  QByteArray              _roverMount;
  QByteArray              _baseMount;
  QByteArray              _corrMount;
  QMap<QString, int>      _IODs;
  std::vector<t_epoData*> _epochs;
};

#endif
