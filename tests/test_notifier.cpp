// Unit tests for SystemStateNotifier (non-FreeRTOS, just the subscribe/broadcast logic)

#include <gtest/gtest.h>
#include <ungula/eventbus/i_system_state_listener.h>
#include <ungula/eventbus/system_state_notifier.h>

// Mock listener that counts notifications
class MockListener : public ungula::eventbus::ISystemStateListener {
public:
    int notifyCount = 0;
    bool started = false;

    bool start() override
    {
        started = true;
        return true;
    }
    void stop() override
    {
        started = false;
    }
    void notifyStateChanged() override
    {
        ++notifyCount;
    }
};

// --- Subscribe / Unsubscribe ---

TEST(SystemStateNotifier, SubscribeReturnsTrue)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    EXPECT_TRUE(notifier.subscribe(&listener));
    EXPECT_EQ(notifier.listenerCount(), 1);
}

TEST(SystemStateNotifier, RejectsDuplicateSubscription)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    EXPECT_TRUE(notifier.subscribe(&listener));
    EXPECT_FALSE(notifier.subscribe(&listener));
    EXPECT_EQ(notifier.listenerCount(), 1);
}

TEST(SystemStateNotifier, RejectsNullPointer)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    EXPECT_FALSE(notifier.subscribe(nullptr));
    EXPECT_EQ(notifier.listenerCount(), 0);
}

TEST(SystemStateNotifier, RejectsWhenFull)
{
    ungula::eventbus::SystemStateNotifier<2> notifier;
    MockListener listener1, listener2, listener3;
    EXPECT_TRUE(notifier.subscribe(&listener1));
    EXPECT_TRUE(notifier.subscribe(&listener2));
    EXPECT_FALSE(notifier.subscribe(&listener3));
}

TEST(SystemStateNotifier, UnsubscribeRemovesListener)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    EXPECT_TRUE(notifier.unsubscribe(&listener));
    EXPECT_EQ(notifier.listenerCount(), 0);
}

TEST(SystemStateNotifier, UnsubscribeReturnsFalseIfNotFound)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    EXPECT_FALSE(notifier.unsubscribe(&listener));
}

// --- Notify ---

TEST(SystemStateNotifier, NotifyDoesNothingWhenInactive)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    notifier.notify(); // inactive by default
    EXPECT_EQ(listener.notifyCount, 0);
}

TEST(SystemStateNotifier, NotifyBroadcastsWhenActive)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    notifier.activate();
    notifier.notify();
    EXPECT_EQ(listener.notifyCount, 1);
}

TEST(SystemStateNotifier, NotifyBroadcastsToAllListeners)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener1, listener2, listener3;
    notifier.subscribe(&listener1);
    notifier.subscribe(&listener2);
    notifier.subscribe(&listener3);
    notifier.activate();
    notifier.notify();
    EXPECT_EQ(listener1.notifyCount, 1);
    EXPECT_EQ(listener2.notifyCount, 1);
    EXPECT_EQ(listener3.notifyCount, 1);
}

TEST(SystemStateNotifier, MultipleNotifyCalls)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    notifier.activate();
    notifier.notify();
    notifier.notify();
    notifier.notify();
    EXPECT_EQ(listener.notifyCount, 3);
}

// --- Activate / Deactivate ---

TEST(SystemStateNotifier, DeactivateStopsNotifications)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    notifier.activate();
    notifier.notify();
    EXPECT_EQ(listener.notifyCount, 1);
    notifier.deactivate();
    notifier.notify();
    EXPECT_EQ(listener.notifyCount, 1); // no increase
}

TEST(SystemStateNotifier, ReactivateResumesNotifications)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener;
    notifier.subscribe(&listener);
    notifier.activate();
    notifier.notify();
    notifier.deactivate();
    notifier.notify();
    notifier.activate();
    notifier.notify();
    EXPECT_EQ(listener.notifyCount, 2);
}

// --- Unsubscribe during active ---

TEST(SystemStateNotifier, UnsubscribedListenerDoesNotReceive)
{
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MockListener listener1, listener2;
    notifier.subscribe(&listener1);
    notifier.subscribe(&listener2);
    notifier.activate();
    notifier.notify();
    EXPECT_EQ(listener1.notifyCount, 1);
    EXPECT_EQ(listener2.notifyCount, 1);
    notifier.unsubscribe(&listener1);
    notifier.notify();
    EXPECT_EQ(listener1.notifyCount, 1); // no increase
    EXPECT_EQ(listener2.notifyCount, 2);
}
