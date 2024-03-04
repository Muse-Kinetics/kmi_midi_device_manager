// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef READONLYSLIDER_H
#define READONLYSLIDER_H

#include <QSlider>

class ReadOnlySlider : public QSlider {
    Q_OBJECT
public:
    using QSlider::QSlider;  // Inherit constructors

protected:
    void mousePressEvent(QMouseEvent *event) override {
        Q_UNUSED(event);
    }


};


#endif // READONLYSLIDER_H
