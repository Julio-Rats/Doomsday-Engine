/** @file networkinterfaces.cpp  Information about network interfaces.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "../src/net/networkinterfaces.h"
#include "de/Task"
#include "de/TaskPool"
#include "de/Address"

#include <the_Foundation/object.h>
#include <the_Foundation/objectlist.h>

namespace de {
namespace internal {

static const ddouble UPDATE_THRESHOLD = 60.0;

DE_PIMPL_NOREF(NetworkInterfaces), public Lockable
{
    Time          lastUpdatedAt;
    List<Address> interfaces;

#if 0
    struct AddressQueryTask : public Task
    {
        NetworkInterfaces::Impl *d;

        AddressQueryTask(NetworkInterfaces::Impl *d)
            : d(d)
        {}
        void runTask() override
        {
            List<Address> ipv6;
//            foreach (QHostAddress addr, QNetworkInterface::allAddresses())
//            {
//                ipv6 << QHostAddress(addr.toIPv6Address());
//            }

            // Submit the updated information.
            {
                DE_GUARD(d);
                d->addresses = ipv6;
                d->gotAddresses = true;
            }

            //qDebug() << "Local IP addresses:" << ipv6;
        }
    };

    List<Address> addresses; // lock before access
    TaskPool tasks;
//    QTimer updateTimer;
    bool gotAddresses = false;

    Impl(Public *i) : Base(i)
    {
//        updateTimer.setInterval(1000 * 60 * 1);
//        updateTimer.setSingleShot(false);
//        QObject::connect(&updateTimer, &QTimer::timeout, [this] ()
//        {
//            tasks.start(new AddressQueryTask(this));
//        });
//        updateTimer.start();
        tasks.start(new AddressQueryTask(this));
    }
#endif

    void update()
    {
        interfaces.clear();
        auto infs = tF::make_ref(networkInterfaces_Address());
        iConstForEach(ObjectList, i, infs)
        {
            interfaces << Address(reinterpret_cast<const iAddress *>(i.object));
        }
        lastUpdatedAt = Time();
    }
};

NetworkInterfaces::NetworkInterfaces()
    : d(new Impl)
{
    DE_GUARD(d);
    d->update();
}

List<Address> NetworkInterfaces::allAddresses() const
{
    DE_GUARD(d);
    if (d->lastUpdatedAt.since() > UPDATE_THRESHOLD)
    {
        d->update();
    }
    return d->interfaces;
}

NetworkInterfaces &NetworkInterfaces::get() // static
{
    static NetworkInterfaces nif;
    return nif;
}

} // namespace internal
} // namespace de
