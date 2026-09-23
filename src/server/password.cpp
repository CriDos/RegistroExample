#include "password.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>
#include <QtGlobal>

namespace registro {

namespace {

constexpr int kPbkdf2Iterations = 100000;
constexpr int kMaxAcceptedIterations = 1000000;
constexpr int kDkLen = 32;

int pbkdf2Iterations()
{
#ifdef QT_DEBUG
    const int overridden = qEnvironmentVariableIntValue("REGISTRO_PBKDF2_ITERATIONS");
    if (overridden > 0)
        return qMin(overridden, kMaxAcceptedIterations);
#endif
    return kPbkdf2Iterations;
}

QByteArray randomBytes(int n)
{
    QByteArray out(n, Qt::Uninitialized);
    for (int i = 0; i < n; i += 4) {
        const quint32 v = QRandomGenerator::system()->generate();
        memcpy(out.data() + i, &v, qMin(4, n - i));
    }
    return out;
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    quint8 diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<quint8>(a.at(i)) ^ static_cast<quint8>(b.at(i));
    return diff == 0;
}

} // namespace

QString hashPassword(const QString &password)
{
    const int iterations = pbkdf2Iterations();
    const QByteArray salt = randomBytes(16);
    const QByteArray dk = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                                             password.toUtf8(), salt, iterations,
                                                             kDkLen);
    return QStringLiteral("pbkdf2$%1$%2$%3")
        .arg(iterations)
        .arg(QLatin1String(salt.toHex()))
        .arg(QLatin1String(dk.toHex()));
}

bool verifyPassword(const QString &password, const QString &stored)
{
    const QStringList parts = stored.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != QLatin1String("pbkdf2"))
        return false;
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0 || iterations > kMaxAcceptedIterations)
        return false;
    const QByteArray salt = QByteArray::fromHex(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromHex(parts.at(3).toLatin1());
    if (salt.isEmpty() || expected.isEmpty())
        return false;
    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), salt, iterations, expected.size());
    return constantTimeEquals(derived, expected);
}

QString generateToken()
{
    return QLatin1String(randomBytes(32).toHex());
}

QString hashToken(const QString &token)
{
    return QLatin1String(
        QCryptographicHash::hash(token.toLatin1(), QCryptographicHash::Sha256).toHex());
}

} // namespace registro