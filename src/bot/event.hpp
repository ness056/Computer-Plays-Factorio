#pragma once

#include <QObject>

namespace ComputerPlaysFactorio {

    class EventManager : public QObject {
        Q_OBJECT

    public:
        inline void NotifyNewInstruction() const {
            emit NewInstruction();
        }

    signals:
        void NewInstruction() const;  // Emitted when NotifyNewInstruction is called
    };
}