import os
import threading
import traceback


def waiter(event: threading.Event) -> None:
    event.wait()


def joiner(thr: threading.Thread) -> None:
    try:
        thr.join()
    except AssertionError as exc:
        traceback.print_exc()
        os._exit(1)


def repro() -> None:
    event = threading.Event()
    threads = []
    threads.append(threading.Thread(target=waiter, name="join-race-A", args=(event,)))
    threads.append(threading.Thread(target=joiner, name="join-race-B", args=(threads[0],)))
    threads.append(threading.Thread(target=joiner, name="join-race-C", args=(threads[0],)))
    for thr in threads:
        thr.start()
    # Unblock waiter
    event.set()

    # Wait for joiners to exit. We must allow the joiner threads to wait first,
    # otherwise we may wait on the _tstate_lock first, acquire it, and set it
    # to None before either of the joiners have a chance to do so.
    threads[1].join()
    threads[2].join()

    # Wait for waiter to exit
    threads[0].join()


def main() -> None:
    for i in range(1000):
        print(f"=== Attempt {i} ===")
        repro()
        print()


if __name__ == "__main__":
    main()
