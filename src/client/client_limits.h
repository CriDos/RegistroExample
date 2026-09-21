#pragma once

#include "client.h"

#include <QObject>
#include <QString>

namespace registro {

class Limits : public QObject {
    Q_OBJECT
    Q_PROPERTY(int fullNameMax READ fullNameMax CONSTANT)
    Q_PROPERTY(int orgMax READ orgMax CONSTANT)
    Q_PROPERTY(int phoneMax READ phoneMax CONSTANT)
    Q_PROPERTY(int emailMax READ emailMax CONSTANT)
    Q_PROPERTY(int noteMax READ noteMax CONSTANT)
    Q_PROPERTY(QString statusActive READ statusActive CONSTANT)
    Q_PROPERTY(QString statusArchived READ statusArchived CONSTANT)

public:
    int fullNameMax() const { return kFullNameMax; }
    int orgMax() const { return kOrgMax; }
    int phoneMax() const { return kPhoneMax; }
    int emailMax() const { return kEmailMax; }
    int noteMax() const { return kNotesMax; }
    QString statusActive() const { return QString::fromLatin1(kStatusActive); }
    QString statusArchived() const { return QString::fromLatin1(kStatusArchived); }
};

} // namespace registro