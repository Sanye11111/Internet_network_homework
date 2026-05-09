import random
import threading
import time

TOTAL = 20
WINDOW_SIZE = 5
LOSS_RATE = 0.25

lock = threading.Lock()
base = 0
next_seq = 0
acked = [False] * TOTAL
received = [False] * TOTAL
running = True


def sender():
    global next_seq, running
    while True:
        with lock:
            if base >= TOTAL:
                running = False
                break
            while next_seq < TOTAL and next_seq < base + WINDOW_SIZE:
                if random.random() >= LOSS_RATE:
                    received[next_seq] = True
                    print("send packet", next_seq)
                else:
                    print("packet", next_seq, "lost")
                next_seq += 1
        time.sleep(1)


def receiver():
    global base
    while running:
        with lock:
            for i in range(base, min(base + WINDOW_SIZE, TOTAL)):
                if received[i] and not acked[i]:
                    if random.random() >= LOSS_RATE:
                        acked[i] = True
                        print("ack packet", i)
                    else:
                        print("ack", i, "lost")
            while base < TOTAL and acked[base]:
                base += 1
        time.sleep(1)


def timer():
    global next_seq
    while running:
        time.sleep(1)
        with lock:
            sent_acked = [i for i in range(TOTAL) if i < next_seq and acked[i]]
            sent_no_ack = [i for i in range(TOTAL) if i < next_seq and not acked[i]]
            can_send = [i for i in range(TOTAL) if next_seq <= i < base + WINDOW_SIZE]
            cannot_send = [i for i in range(TOTAL) if i >= base + WINDOW_SIZE]
            delivered = [i for i in range(TOTAL) if received[i]]
            can_receive = [i for i in range(base, min(base + WINDOW_SIZE, TOTAL))]
            cannot_receive = [i for i in range(TOTAL) if i >= base + WINDOW_SIZE]
            usable_window = max(0, base + WINDOW_SIZE - next_seq)

            print("----- window status -----")
            print("sent and acked:", sent_acked)
            print("sent but not acked:", sent_no_ack)
            print("can send but not sent:", can_send)
            print("cannot send:", cannot_send)
            print("acked and delivered:", delivered)
            print("can receive:", can_receive)
            print("cannot receive:", cannot_receive)
            print("advertised window:", WINDOW_SIZE)
            print("usable window:", usable_window)

            if sent_no_ack and random.random() < 0.5:
                resend = sent_no_ack[0]
                print("timeout, resend packet", resend)
                received[resend] = True


if __name__ == "__main__":
    threads = [
        threading.Thread(target=sender),
        threading.Thread(target=receiver),
        threading.Thread(target=timer),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    print("finish")
