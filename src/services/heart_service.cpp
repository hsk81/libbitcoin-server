/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/services/heart_service.hpp>

#include <algorithm>
#include <cstdint>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

static const auto domain = "heartbeat";

using namespace bc::config;
using namespace bc::protocol;

static uint32_t to_milliseconds(uint16_t seconds)
{
    const auto milliseconds = static_cast<uint32_t>(seconds) * 1000;
    return std::min(milliseconds, max_uint32);
};

// Heartbeat is capped at ~ 25 days by signed/millsecond conversions.
heart_service::heart_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    settings_(node.server_settings()),
    period_(to_milliseconds(settings_.heartbeat_interval_seconds)),
    authenticator_(authenticator),
    secure_(secure)
{
}

// Implement service as a publisher.
// The publisher does not block if there are no subscribers or at high water.
void heart_service::work()
{
    zmq::socket publisher(authenticator_, zmq::socket::role::publisher);

    // Bind socket to the worker endpoint.
    if (!started(bind(publisher)))
        return;

    zmq::poller poller;
    poller.add(publisher);

    // Pick a random counter start, will wrap around at overflow.
    auto count = static_cast<uint32_t>(pseudo_random());

    // We will not receive on the poller, we use its timer and context stop.
    while (!poller.terminated() && !stopped())
    {
        poller.wait(period_);
        publish(count++, publisher);
    }

    // Unbind the socket and exit this thread.
    finished(unbind(publisher));
}

void heart_service::publish(uint32_t count, zmq::socket& publisher)
{
    const auto security = secure_ ? "secure" : "public";

    zmq::message message;
    message.enqueue_little_endian(count);
    auto ec = message.send(publisher);

    if (ec && ec != error::service_stopped)
        log::warning(LOG_SERVER)
            << "Failed to publish " << security << " heartbeat: "
            << ec.message();

    // This isn't actually a request, should probably update settings.
    if (!settings_.log_requests)
        return;

    log::debug(LOG_SERVER)
        << "Published " << security << " heartbeat [" << count << "].";
}

// Bind/Unbind.
//-----------------------------------------------------------------------------

bool heart_service::bind(zmq::socket& publisher)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? settings_.secure_heartbeat_endpoint :
        settings_.public_heartbeat_endpoint;

    if (secure_ && !authenticator_.apply(publisher, domain, true))
    {
        log::error(LOG_SERVER)
            << "Failed to apply authenticator to secure heartbeat service.";
        return false;
    }

    const auto ec = publisher.bind(endpoint);

    if (ec)
    {
        log::error(LOG_SERVER)
            << "Failed to bind " << security << " heartbeat service to "
            << endpoint << " : " << ec.message();
        return false;
    }

    log::info(LOG_SERVER)
        << "Bound " << security << " heartbeat service to " << endpoint;
    return true;
}

bool heart_service::unbind(zmq::socket& publisher)
{
    const auto security = secure_ ? "secure" : "public";

    if (!publisher.stop())
    {
        log::error(LOG_SERVER)
            << "Failed to disconnect " << security << " heartbeat worker.";
        return false;
    }

    // Don't log stop success.
    return true;
}

} // namespace server
} // namespace libbitcoin