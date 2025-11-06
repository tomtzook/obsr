#pragma once

#include <cstdint>

namespace obsr {

template<typename state_, state_ first_, typename data_>
class state_machine {
public:
    using process_func = std::function<bool(state_, data_&)>;

    explicit state_machine(process_func&& process_func);
    virtual ~state_machine() = default;

    [[nodiscard]] bool is_finished() const;
    [[nodiscard]] bool is_errored() const;
    [[nodiscard]] uint8_t error_code() const;
    [[nodiscard]] const data_& data() const;

    bool move_to_state(state_ state);
    bool error(uint8_t code);
    bool try_later();
    bool finished();

    void reset();
    void process();

private:
    enum class overall_state {
        start,
        end,
        error,
        in_state
    };

    bool process_once();

    process_func m_process_func;
    overall_state m_state;
    state_ m_user_state;
    uint8_t m_error_code;
    data_ m_data;
};

template<typename state_, state_ first_, typename data_>
state_machine<state_, first_, data_>::state_machine(process_func&& process_func)
    : m_process_func(std::move(process_func))
    , m_state(overall_state::start)
    , m_user_state(first_)
    , m_error_code(0)
{}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::is_finished() const {
    return m_state == overall_state::end;
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::is_errored() const {
    return m_error_code != 0;
}

template<typename state_, state_ first_, typename data_>
uint8_t state_machine<state_, first_, data_>::error_code() const {
    return m_error_code;
}

template<typename state_, state_ first_, typename data_>
const data_& state_machine<state_, first_, data_>::data() const {
    return m_data;
}

template<typename state_, state_ first_, typename data_>
void state_machine<state_, first_, data_>::reset() {
    m_state = overall_state::start;
    m_error_code = 0;
    m_user_state = first_;
}

template<typename state_, state_ first_, typename data_>
void state_machine<state_, first_, data_>::process() {
    switch (m_state) {
        case overall_state::start:
            m_state = overall_state::in_state;
            m_error_code = 0;
            m_user_state = first_;
            break;
        case overall_state::error:
        case overall_state::end:
            return;
        default:
            break;
    }

    while (process_once());
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::move_to_state(state_ state) {
    m_state = overall_state::in_state;
    m_user_state = state;
    return true;
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::error(const uint8_t code) {
    m_state = overall_state::error;
    m_error_code = code;
    return false;
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::try_later() {
    return false;
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::finished() {
    m_state = overall_state::end;
    return false;
}

template<typename state_, state_ first_, typename data_>
bool state_machine<state_, first_, data_>::process_once() {
    switch (m_state) {
        case overall_state::in_state:
            return m_process_func(m_user_state, m_data);
        case overall_state::start:
        case overall_state::error:
        case overall_state::end:
        default:
            return false;
    }
}

}
