/*
M2Mqtt Project - MQTT Client Library for .Net and GnatMQ MQTT Broker for .NET
Copyright (c) 2014, Paolo Patierno, All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.
*/

using System;
using System.Collections;

namespace uPLibrary.Networking.M2Mqtt
{
    /// <summary>
    /// Extension class for a Queue
    /// </summary>
    internal static class QueueExtension
    {
        /// <summary>
        /// Predicate for searching inside a queue
        /// </summary>
        /// <param name="item">Item of the queue</param>
        /// <returns>Result of predicate</returns>
        internal delegate bool QueuePredicate(object item);

        /// <summary>
        /// Get (without removing) an item from queue based on predicate
        /// </summary>
        /// <param name="queue">Queue in which to search</param>
        /// <param name="predicate">Predicate to verify to get item</param>
        /// <returns>Item matches the predicate</returns>
        internal static object Get(this Queue queue, QueuePredicate predicate)
        {
            foreach (var item in queue)
            {
                if (predicate(item))
                    return item;
            }
            return null;
        }
    }
}
