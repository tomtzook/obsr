#pragma once

#include <cstdint>

namespace obsr {

template<typename state_, state_ first_, typename data_>
class state_machine {
public:
    state_machine()
        : m_state(overall_state::start)
        , m_error_code(0)
        , m_user_state(first_)
    {}

    bool is_finished() const {
        return m_state == overall_state::end;
    }

    bool is_errored() const {
        return m_error_code != 0;
    }

    uint8_t error_code() const {
        return m_error_code;
    }

    const data_ data() const {
        return m_data;
    }

    void reset() {
        m_state = overall_state::start;
        m_error_code = 0;
        m_user_state = first_;
    }

    void process() {
        switch (m_state) {
            case overall_state::start:
                m_state = overall_state::in_state;
                m_error_code = 0;
                m_user_state = first_;
                break;
            case overall_state::error:
            case overall_state::end:
            default:
                return;
        }

        while (process_once());
    }

protected:

    virtual bool process_state(state_ current_state, data_& data) = 0;

    inline bool move_to_state(state_ state) {
        m_state = overall_state::in_state;
        m_user_state = state;
        return true;
    }

    inline bool error(uint8_t code) {
        m_state = overall_state::error;
        m_error_code = code;
        return false;
    }

    inline bool try_later() {
        return false;
    }

    inline bool finished() {
        m_state = overall_state::end;
        return false;
    }

private:
    enum class overall_state {
        start,
        end,
        error,
        in_state
    };

    bool process_once() {
        switch (m_state) {
            case overall_state::in_state:
                return process_state(m_user_state, m_data);
            case overall_state::start:
            case overall_state::error:
            case overall_state::end:
            default:
                return false;
        }
    }

    overall_state m_state;
    state_ m_user_state;
    uint8_t m_error_code;
    data_ m_data;
};

}
