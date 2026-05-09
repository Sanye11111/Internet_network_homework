import queue
import random
import threading
import time

LOSS_RATE = 0.25
TOTAL_PACKETS = 10

sender_to_receiver = queue.Queue()
receiver_to_sender = queue.Queue()


def receiver():
    expected = 0
    while expected < TOTAL_PACKETS:
        seq, data = sender_to_receiver.get()
        if random.random() < LOSS_RATE:
            print("receiver: packet", seq, "lost")
            continue

        if seq == expected:
            print("receiver: receive", data)
            receiver_to_sender.put(seq)
            expected += 1
        else:
            print("receiver: duplicate packet", seq)
            receiver_to_sender.put(expected - 1)


def sender():
    seq = 0
    while seq < TOTAL_PACKETS:
        print("sender: send packet", seq)
        sender_to_receiver.put((seq, "data-" + str(seq)))

        try:
            ack = receiver_to_sender.get(timeout=1)
            if random.random() < LOSS_RATE:
                print("sender: ack", ack, "lost")
                continue
            if ack == seq:
                print("sender: receive ack", ack)
                seq += 1
        except queue.Empty:
            print("sender: timeout, resend packet", seq)

        time.sleep(0.5)


if __name__ == "__main__":
    t1 = threading.Thread(target=receiver)
    t2 = threading.Thread(target=sender)
    t1.start()
    t2.start()
    t2.join()
    t1.join()
    print("finish")
