#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/system_error.hpp>
#include <memory>
#include <mutex>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Заказ на приготовление одного хот-дога.
// Все операции над состоянием заказа выполняются внутри его собственного strand,
// поэтому обработчики таймеров и горелок могут вызываться из любых потоков.
class HotDogOrder : public std::enable_shared_from_this<HotDogOrder> {
public:
    HotDogOrder(net::io_context& io, int id, std::shared_ptr<GasCooker> gas_cooker,
                std::shared_ptr<Sausage> sausage, std::shared_ptr<Bread> bread,
                HotDogHandler handler)
        : io_{io}
        , id_{id}
        , gas_cooker_{std::move(gas_cooker)}
        , sausage_{std::move(sausage)}
        , bread_{std::move(bread)}
        , handler_{std::move(handler)} {
    }

    // Запускает асинхронное приготовление хот-дога
    void Execute() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->FrySausage();
            self->BakeBread();
        });
    }

private:
    void FrySausage() {
        // Обработчик UseBurner вызывается через io_ в произвольном потоке,
        // поэтому переходим в strand заказа
        sausage_->StartFry(*gas_cooker_, [self = shared_from_this()] {
            net::dispatch(self->strand_, [self] {
                self->OnSausageFryStarted();
            });
        });
    }

    void OnSausageFryStarted() {
        sausage_timer_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
        sausage_timer_.async_wait(
            net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
                self->OnSausageFried(ec);
            }));
    }

    void OnSausageFried(sys::error_code ec) {
        sausage_->StopFry();
        if (ec) {
            return Fail(ec);
        }
        CheckReadiness();
    }

    void BakeBread() {
        bread_->StartBake(*gas_cooker_, [self = shared_from_this()] {
            net::dispatch(self->strand_, [self] {
                self->OnBreadBakeStarted();
            });
        });
    }

    void OnBreadBakeStarted() {
        bread_timer_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
        bread_timer_.async_wait(
            net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
                self->OnBreadBaked(ec);
            }));
    }

    void OnBreadBaked(sys::error_code ec) {
        bread_->StopBaking();
        if (ec) {
            return Fail(ec);
        }
        CheckReadiness();
    }

    // Собирает хот-дог, когда оба ингредиента готовы
    void CheckReadiness() {
        if (delivered_ || !sausage_->IsCooked() || !bread_->IsCooked()) {
            return;
        }
        delivered_ = true;
        try {
            handler_(HotDog{id_, sausage_, bread_});
        } catch (...) {
            // HotDog бросает исключение, если ингредиенты приготовлены неправильно
            handler_(Result<HotDog>::FromCurrentException());
        }
    }

    void Fail(sys::error_code ec) {
        if (delivered_) {
            return;
        }
        delivered_ = true;
        handler_(Result<HotDog>{std::make_exception_ptr(sys::system_error{ec})});
    }

    using Strand = net::strand<net::io_context::executor_type>;

    net::io_context& io_;
    Strand strand_{net::make_strand(io_)};
    int id_;
    std::shared_ptr<GasCooker> gas_cooker_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    HotDogHandler handler_;
    net::steady_timer sausage_timer_{strand_};
    net::steady_timer bread_timer_{strand_};
    bool delivered_ = false;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        std::shared_ptr<Sausage> sausage;
        std::shared_ptr<Bread> bread;
        int order_id = 0;
        {
            // Store не потокобезопасен, поэтому защищаем обращение к нему мьютексом
            std::lock_guard lock{mutex_};
            sausage = store_.GetSausage();
            bread = store_.GetBread();
            order_id = ++next_order_id_;
        }

        std::make_shared<HotDogOrder>(io_, order_id, gas_cooker_, std::move(sausage),
                                      std::move(bread), std::move(handler))
            ->Execute();
    }

private:
    net::io_context& io_;
    std::mutex mutex_;
    int next_order_id_ = 0;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
