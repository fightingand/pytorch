import unittest
import os

IN_CI = os.environ.get("CI")

from tools.stats.upload_test_stats import get_test_cases


class TestUploadTestStats(unittest.TestCase):
    @unittest.skipIf(
        IN_CI,
        "don't run in CI as this does a lot of network calls and uses up GH API rate limit",
    )
    def test_existing_job(self) -> None:
        """Run on a known-good job and make sure we don't error and get basically okay reults."""
        test_cases = get_test_cases(2465214458, 1)
        self.assertEqual(len(test_cases), 731457)


if __name__ == "__main__":
    unittest.main()
