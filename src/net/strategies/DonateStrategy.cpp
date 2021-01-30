/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018-2020 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2020 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "net/strategies/DonateStrategy.h"
#include "3rdparty/rapidjson/document.h"
#include "base/crypto/keccak.h"
#include "base/kernel/interfaces/IStrategyListener.h"
#include "base/kernel/Platform.h"
#include "base/net/stratum/Client.h"
#include "base/tools/Buffer.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "donate.h"
#include "proxy/Counters.h"
#include "proxy/StatsData.h"


namespace xmrig {


static inline double randomf(double min, double max)    { return (max - min) * (((static_cast<double>(rand())) / static_cast<double>(RAND_MAX))) + min; }


} // namespace xmrig


xmrig::DonateStrategy::DonateStrategy(Controller *controller, IStrategyListener *listener) :
    m_active(false),
    m_controller(controller),
    m_listener(listener),
    m_donateTicks(0),
    m_target(0),
    m_ticks(0)
{

        
}


xmrig::DonateStrategy::~DonateStrategy()
{
    m_client->deleteLater();
}


bool xmrig::DonateStrategy::reschedule()
{
    const uint64_t level = m_controller->config()->pools().donateLevel() * 60;
    if (m_donateTicks < level) {
        return false;
    }

    m_target = m_ticks + ((6000 - level) * ((double) m_donateTicks / level));
    m_active = false;

    stop();
    return true;
}


void xmrig::DonateStrategy::save(const IClient *client, const Job &job)
{
    m_pending.job  = job;
    m_pending.host = client->pool().host();
    m_pending.port = client->pool().port();
}


void xmrig::DonateStrategy::setAlgo(const xmrig::Algorithm &algorithm)
{
    m_client->setAlgo(algorithm);
}


int64_t xmrig::DonateStrategy::submit(const JobResult &result)
{
    return m_client->submit(result);
}


void xmrig::DonateStrategy::connect()
{
}


void xmrig::DonateStrategy::stop()
{
    m_donateTicks = 0;
    m_client->disconnect();
}


void xmrig::DonateStrategy::tick(uint64_t now)
{
    m_client->tick(now);

    m_ticks++;

    if (m_ticks == m_target) {
        m_pending.job.reset();
        m_client->connect();
    }

    if (isActive()) {
        m_donateTicks++;
    }
}


void xmrig::DonateStrategy::onClose(IClient *, int)
{
    if (!isActive()) {
        return;
    }

    m_active = false;
    m_listener->onPause(this);
}


void xmrig::DonateStrategy::onJobReceived(IClient *client, const Job &job, const rapidjson::Value &params)
{
    if (!isActive()) {
        m_active = true;
        m_listener->onActive(this, client);
    }

    m_listener->onJob(this, client, job, params);
}


void xmrig::DonateStrategy::onLogin(IClient *, rapidjson::Document &doc, rapidjson::Value &params)
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    Value algo(kArrayType);
    algo.PushBack(m_client->pool().algorithm().toJSON(), allocator);

    params.AddMember("algo", algo, allocator);
}


void xmrig::DonateStrategy::onLoginSuccess(IClient *)
{
}


void xmrig::DonateStrategy::onResultAccepted(IClient *client, const SubmitResult &result, const char *error)
{
    m_listener->onResultAccepted(this, client, result, error);
}


void xmrig::DonateStrategy::onVerifyAlgorithm(const IClient *, const Algorithm &, bool *)
{

}


void xmrig::DonateStrategy::setProxy(const ProxyUrl &proxy)
{
    m_client->setProxy(proxy);
}
