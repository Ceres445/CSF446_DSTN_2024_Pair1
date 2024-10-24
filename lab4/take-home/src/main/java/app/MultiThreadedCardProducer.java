package app;

import org.apache.kafka.clients.producer.Producer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * MultiThreadedCardProducer is a class designed to produce Card objects to a Kafka topic
 * using multiple threads to enhance throughput. It partitions the cards based on their IDs
 * and distributes the production tasks across a fixed number of threads.
 */
public class MultiThreadedCardProducer {

    private final int numPartitions; // DONT CHANGE

    private final SimpleCardProducer producer; // DONT CHANGE
    private final ExecutorService executorService;


    /**
     * Constructs a MultiThreadedCardProducer with a specified number of partitions.
     *
     * @param numPartitions number of partitions.
     */
    public MultiThreadedCardProducer(int numPartitions) {
        this.producer = new SimpleCardProducer();
        this.numPartitions = numPartitions;
        this.executorService = Executors.newFixedThreadPool(numPartitions);
    }

    /**
     * Constructs a MultiThreadedCardProducer with a specified number of partitions and a custom producer.
     *
     * @param numPartitions number of partitions.
     * @param producer Kafka Producer instance to be used for producing messages.
     */
    public MultiThreadedCardProducer(int numPartitions, Producer<String, String> producer) {
        this.producer = new SimpleCardProducer(producer);
        this.numPartitions = numPartitions;
        this.executorService = Executors.newFixedThreadPool(numPartitions);
    }

    /**
     * Produces a list of Cards to the specified Kafka topic using multiple threads.
     * ensures even distribution of Cards to partitions.
     * ensures all threads complete before returning.
     *
     * @param cards list of Card objects to be produced.
     * @param topic Kafka topic to which the cards will be sent.
     */
    public void produceCards(List<Card> cards, String topic) {
        try {
            // Group cards by their target partition
            List<List<Card>> partitionedCards = partitionCards(cards);

            // Submit production tasks for each partition
            for (int partition = 0; partition < numPartitions; partition++) {
                final int currentPartition = partition;
                final List<Card> cardsForPartition = partitionedCards.get(partition);

                if (!cardsForPartition.isEmpty()) {
                    executorService.submit(() -> {
                        try {
                            System.out.printf("Thread %s starting to produce %d cards to partition %d%n",
                                    Thread.currentThread().getName(),
                                    cardsForPartition.size(),
                                    currentPartition);

                            for (Card card : cardsForPartition) {
                                producer.produceCard(card, topic, currentPartition);
                            }

                            System.out.printf("Thread %s completed producing cards to partition %d%n",
                                    Thread.currentThread().getName(),
                                    currentPartition);
                        } catch (Exception e) {
                            System.err.printf("Error in producer thread for partition %d: %s%n",
                                    currentPartition, e.getMessage());
                        }
                    });
                }
            }

            // Initiate shutdown but don't terminate existing tasks
            executorService.shutdown();

            // Wait for all tasks to complete or timeout after 1 minute
            if (!executorService.awaitTermination(1, TimeUnit.MINUTES)) {
                System.err.println("Timeout waiting for producer threads to complete");
                executorService.shutdownNow();
            }

        } catch (Exception e) {
            System.err.println("Error in multi-threaded production: " + e.getMessage());
            executorService.shutdownNow();
        }
    }

    /**
     * Flushes pending records to Kafka broker.
     */
    public void flush() {
        try {
            producer.flush();
        } catch (Exception e) {
            System.err.println("Error flushing producer: " + e.getMessage());
        }
    }

    /**
     * close the producer and release any resources held by the class
     */
    public void close() {
        try {
            // Ensure executor service is shut down
            if (!executorService.isShutdown()) {
                executorService.shutdown();
                if (!executorService.awaitTermination(30, TimeUnit.SECONDS)) {
                    executorService.shutdownNow();
                }
            }

            // Close the producer
            producer.close();
            System.out.println("MultiThreadedCardProducer closed successfully");
        } catch (Exception e) {
            System.err.println("Error closing MultiThreadedCardProducer: " + e.getMessage());
            executorService.shutdownNow();
        }
    }


    private List<List<Card>> partitionCards(List<Card> cards) {
        // Initialize partition lists
        List<List<Card>> partitionedCards = new ArrayList<>(numPartitions);
        for (int i = 0; i < numPartitions; i++) {
            partitionedCards.add(new ArrayList<>());
        }

        // Distribute cards across partitions based on their ID
        for (Card card : cards) {
            int partition = Math.abs(card.id % numPartitions);
            partitionedCards.get(partition).add(card);
        }

        return partitionedCards;
    }


    public static void main(String[] args) {
        MultiThreadedCardProducer multiThreadedProducer = new MultiThreadedCardProducer(8);

        int numCards = 100;
        List<Card> cards = Card.getRandomCards(numCards);
        long start = System.currentTimeMillis();
        multiThreadedProducer.produceCards(cards, "cards-topic");
        multiThreadedProducer.flush();
        long end = System.currentTimeMillis();

        System.out.println("Produced " + numCards + " cards in " + (end - start) + "ms");

        multiThreadedProducer.close();
    }
}
