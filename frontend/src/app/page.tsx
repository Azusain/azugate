import { Button, ChakraProvider } from "@chakra-ui/react";

export default function Home() {
  return (
    <ChakraProvider>
      <div>
        <Button> Click Me </Button>
      </div>
    </ChakraProvider>
  );
}
