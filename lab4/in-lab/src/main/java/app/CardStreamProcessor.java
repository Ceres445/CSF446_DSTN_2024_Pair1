package app;

import com.google.gson.Gson;
import org.apache.kafka.streams.KafkaStreams;
import org.apache.kafka.streams.KeyValue;
import org.apache.kafka.streams.StreamsBuilder;
import org.apache.kafka.streams.Topology;
import org.apache.kafka.streams.kstream.*;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.time.Duration;
import java.util.Properties;
import java.util.UUID;

/**
 * The CardStreamProcessor class processes streams of card objects represented as GSON strings.
 * If there are duplicate cards in a 30 millisecond time window it flags
 * these duplicates by sending them to an output Kafka topic.
 */
public class CardStreamProcessor {

    private boolean inDocker = new File("/.dockerenv").exists();
    private static final Gson gson = new Gson();

    /**
     * Builds the Kafka Streams topology for processing card streams. The topology reads from the input
     * topic and writes flagged duplicates to the output topic.
     *
     * @param inputTopic  the Kafka topic from which card objects are read
     * @param outputTopic the Kafka topic to which flagged duplicate cards are written
     * @return the Kafka Streams topology for processing card objects
     */
    public Topology buildTopology(String inputTopic, String outputTopic) {
        StreamsBuilder builder = new StreamsBuilder();

        // Create a stream from the input topic
        KStream<String, String> inputStream = builder.stream(inputTopic);

        // Group by card JSON and create sliding windows
        KStream<Windowed<String>, Long> counts = inputStream
                .groupBy((key, value) -> value)
                .windowedBy(SlidingWindows.ofTimeDifferenceWithNoGrace(Duration.ofMillis(30)))
                .count()
                .toStream();

        // Flag cards when they first appear as duplicates
        KStream<String, String> flaggedCards = counts
                .filter((windowedKey, count) -> count == 2)  // Only when count exactly equals 2
                .map((windowedKey, count) -> {
                    String cardJson = windowedKey.key();
                    return KeyValue.pair(cardJson, cardJson);
                });

        // Write to output topic
        flaggedCards.to(outputTopic);

        return builder.build();
    }

    /**
     * Creates a KafkaStreams instance with the provided topology and properties.
     *
     * @param topology the Kafka Streams topology defining the stream processing logic
     * @param props    the properties for configuring the Kafka Streams instance
     * @return a KafkaStreams instance ready to start processing streams
     */
    public KafkaStreams createKafkaStreams(Topology topology, Properties props) {
        // Create and return a new KafkaStreams instance with the topology and properties
        return new KafkaStreams(topology, props);
    }

    /**
     * Creates and configures a Kafka producer instance using properties loaded from a file.
     * This method checks if the application is running in a Docker environment and adjusts
     * the Kafka bootstrap servers accordingly.
     *
     * @return a Properties object containing the configuration for the Kafka Streams application
     */
    private Properties getProperties() throws IOException { // DONT CHANGE
        Properties props = new Properties();
        try (InputStream stream =
                     CardStreamProcessor.class.getClassLoader().getResourceAsStream("streams.properties")) {

            props.load(stream);

            props.setProperty("application.id", props.getProperty("application.id.prefix") + "-" + UUID.randomUUID());
            props.setProperty("client.id", props.getProperty("application.id"));
            props.setProperty("group.instance.id", props.getProperty("application.id"));

            if (inDocker) {
                props.setProperty("bootstrap.servers", props.getProperty("bootstrap.servers.docker"));
            }

            return props;
        }
    }


    public static void main(String[] args) throws IOException {
        CardStreamProcessor processor = new CardStreamProcessor();
        Properties props = processor.getProperties();
        String inputTopic = "cards-topic";
        String outputTopic = "cards-flag-topic";

        Topology topology = processor.buildTopology(inputTopic, outputTopic);
        KafkaStreams streams = processor.createKafkaStreams(topology, props);

        System.out.println("Starting Kafka Streams");
        streams.start();

        Runtime.getRuntime().addShutdownHook(new Thread(streams::close));
    }
}
